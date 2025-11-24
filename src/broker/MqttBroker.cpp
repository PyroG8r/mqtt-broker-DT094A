#include "MqttBroker.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <algorithm>

namespace mqtt {

MqttBroker::MqttBroker() : serverSocket(-1), running(false) {}

MqttBroker::~MqttBroker() {
    stop();
}

void MqttBroker::start() {
    // Create TCP socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    // Set socket options to reuse address
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket to port 1883
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    
    bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    
    // Start listening
    listen(serverSocket, MAX_CONNECTIONS);
    
    running = true;
    std::cout << "MQTT Broker started on port " << DEFAULT_PORT << std::endl;
}

void MqttBroker::stop() {
    running = false;
    
    // Close all client connections
    for (auto& client : clients) {
        client->disconnect();
    }
    clients.clear();
    
    // Close server socket
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
    
    std::cout << "MQTT Broker stopped." << std::endl;
}

void MqttBroker::run() {
    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        
        int maxFd = serverSocket;
        
        // Add all client sockets to the set
        for (auto& client : clients) {
            int clientFd = client->getSocket();
            if (clientFd > 0) {
                FD_SET(clientFd, &readfds);
                if (clientFd > maxFd) {
                    maxFd = clientFd;
                }
            }
        }
        
        // Wait for activity with timeout
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(maxFd + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (activity < 0) {
            std::cerr << "Select error" << std::endl;
            continue;
        }
        
        // Check for new connections
        if (FD_ISSET(serverSocket, &readfds)) {
            acceptNewConnection();
        }
        
        // Check for data from existing clients
        for (auto it = clients.begin(); it != clients.end();) {
            auto client = *it;
            int clientFd = client->getSocket();
            
            if (FD_ISSET(clientFd, &readfds)) {
                handleClientData(client);
                
                // Check if client disconnected
                if (!client->isConnected()) {
                    std::cout << "Client disconnected" << std::endl;
                    it = clients.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
}

void MqttBroker::acceptNewConnection() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientSocket < 0) {
        std::cerr << "Failed to accept connection" << std::endl;
        return;
    }
    
    std::cout << "New connection accepted from " 
              << inet_ntoa(clientAddr.sin_addr) << ":" 
              << ntohs(clientAddr.sin_port) << std::endl;
    
    auto client = std::make_shared<Connection>(clientSocket);
    clients.push_back(client);
}

void MqttBroker::handleClientData(std::shared_ptr<Connection> client) {
    std::vector<uint8_t> buffer = client->receive();
    
    if (buffer.empty()) {
        // Client disconnected
        client->disconnect();
        return;
    }
    
    // Log received data
    std::cout << "Received " << buffer.size() << " bytes: ";
    for (size_t i = 0; i < std::min(buffer.size(), size_t(16)); ++i) {
        printf("%02X ", buffer[i]);
    }
    if (buffer.size() > 16) {
        std::cout << "...";
    }
    std::cout << std::endl;
}

} // namespace mqtt