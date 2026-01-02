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

MqttBroker::MqttBroker() : serverSocket(-1), running(false), metrics_(std::make_unique<BrokerMetrics>()) {}

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
    
    // Start Prometheus metrics exporter
    metrics_->startExporter("0.0.0.0:9090");
    
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
                    std::cout << "Client disconnected, " << clientFd << ". Cleaning up subscriptions." << std::endl;
                    cleanupClientSubscriptions(client);
                    it = clients.erase(it);
                    
                    // Update metrics
                    metrics_->setActiveConnections(clients.size());
                    metrics_->setActiveSubscriptions(getTotalSubscriptions());
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
    
    // Update metrics
    metrics_->incrementTotalConnections();
    metrics_->setActiveConnections(clients.size());
}

void MqttBroker::handleClientData(std::shared_ptr<Connection> client) {
    std::vector<uint8_t> buffer = client->receive();
    
    if (buffer.empty()) {
        // Client disconnected
        if (!client->hasReceivedData()) {
            // Port probe or connection without MQTT handshake - suppress noisy logging
            // This is common in Docker environments
        } else {
            std::cout << "Client disconnected ungracefully" << std::endl;
        }
        client->disconnect();
        return;
    }
    
    // Track bytes received
    metrics_->incrementBytesReceived(buffer.size());
    
    try {
        MqttPacket packet = MqttPacket::parse(buffer);
        PacketType type = packet.get_packet_type();
        
        switch (type) {
            case PacketType::CONNECT:
                handleConnect(client, packet);
                break;
                
            case PacketType::PUBLISH:
                handlePublish(client, packet);
                break;
                
            case PacketType::SUBSCRIBE:
                handleSubscribe(client, packet);
                break;
                
            case PacketType::UNSUBSCRIBE:
                handleUnsubscribe(client, packet);
                break;
                
            case PacketType::PINGREQ:
                handlePingreq(client);
                break;
                
            case PacketType::DISCONNECT:
                handleDisconnect(client);
                break;
                
            default:
                std::cout << "Unsupported packet type: " << static_cast<int>(type) << std::endl;
                break;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing packet: " << e.what() << std::endl;
        metrics_->incrementConnectionErrors();
        client->disconnect();
    }
}

void MqttBroker::handleConnect(std::shared_ptr<Connection> client, const MqttPacket& packet) {
    std::cout << "Handling CONNECT packet" << std::endl;
    
    try {
        // Parse CONNECT packet
        ConnectPacket connect = ConnectPacket::parse(packet);
        
        std::cout << "Protocol: " << connect.protocol_name << " v" 
                  << static_cast<int>(connect.protocol_version) << std::endl;
        
        // TODO: Validate protocol version (should be 5 for MQTT 5.0)
        // TODO: Check client_id, handle clean session, etc.
        
        // Send CONNACK - successful connection
        MqttPacket connack = PacketFactory::create_connack(0, 0);  // session_present=0, reason_code=0 (success)
        std::vector<uint8_t> response = connack.serialize();
        client->send(response);
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling CONNECT: " << e.what() << std::endl;
        
        // Send CONNACK with error
        MqttPacket connack = PacketFactory::create_connack(0, 0x80);  // Unspecified error
        std::vector<uint8_t> response = connack.serialize();
        client->send(response);
        client->disconnect();
    }
}

void MqttBroker::handlePublish(std::shared_ptr<Connection> client, const MqttPacket& packet) {
    std::cout << "Handling PUBLISH packet" << std::endl;
    
    try {
        // Parse PUBLISH packet
        PublishPacket publish = PublishPacket::parse(packet);
        
        std::cout << "Topic: " << publish.topic_name << std::endl;
        std::cout << "Message: " << std::string(publish.message.begin(), publish.message.end()) << std::endl;
        
        // Track metrics
        metrics_->incrementMessagesReceived();
        metrics_->observeMessageSize(publish.message.size());
        
        // Handle retained messages
        if (packet.get_retain_flag()) {
            retainedMessages[publish.topic_name] = {publish.message, static_cast<uint8_t>(packet.get_qos())}; // Store retained message, overwrite existing
            std::cout << "Stored retained message for topic: " << publish.topic_name << std::endl;
        }
        
        // Forward message to all subscribers
        if (subscriptions.find(publish.topic_name) != subscriptions.end()) { // has subscribers
            for (auto& subscriber : subscriptions[publish.topic_name]) {
                if (subscriber->isConnected()) { // send to all connected subscribers

                    MqttPacket forward = PacketFactory::create_publish(
                        publish.topic_name, 
                        publish.message, 
                        packet.get_qos(), 
                        false,  // Don't forward retain flag
                        0
                    );
                    std::vector<uint8_t> data = forward.serialize();
                    subscriber->send(data);
                    
                    // Track bytes sent and messages published
                    metrics_->incrementBytesSent(data.size());
                    metrics_->incrementMessagesPublished();
                    
                    std::cout << "Forwarded message to subscribers" << std::endl;
                }
            }
        }
        
        // Send PUBACK if QoS > 0
        if (packet.get_qos() == QoSLevel::AT_LEAST_ONCE) {
            MqttPacket puback = PacketFactory::create_puback(publish.packet_identifier, 0);
            std::vector<uint8_t> response = puback.serialize();
            client->send(response);
            std::cout << "Sent PUBACK" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling PUBLISH: " << e.what() << std::endl;
    }
}

void MqttBroker::handleSubscribe(std::shared_ptr<Connection> client, const MqttPacket& packet) {
    std::cout << "Handling SUBSCRIBE packet" << std::endl;
    
    try {
        SubscribePacket subscribe = SubscribePacket::parse(packet);
        
        std::vector<uint8_t> reason_codes;
        
        for (const auto& [topic, qos] : subscribe.topic_filters) {
            std::cout << "Subscribe to topic: " << topic << " (QoS " << static_cast<int>(qos) << ")" << std::endl;
            
            // Add client to subscription list
            subscriptions[topic].push_back(client);
            
            // Send retained message if exists
            if (retainedMessages.find(topic) != retainedMessages.end()) {
                const auto& [message, retain_qos] = retainedMessages[topic];
                MqttPacket retained = PacketFactory::create_publish(
                    topic,
                    message,
                    static_cast<QoSLevel>(retain_qos),
                    true,                                   // Retain flag
                    0                                       // Packet ID not needed for QoS 0
                );
                std::vector<uint8_t> data = retained.serialize();
                client->send(data);
                std::cout << "Sent retained message for topic: " << topic << std::endl;
            }
            
            // Success - granted QoS
            reason_codes.push_back(qos);
        }
        
        // Send SUBACK
        MqttPacket suback = PacketFactory::create_suback(subscribe.packet_identifier, reason_codes);
        std::vector<uint8_t> response = suback.serialize();
        client->send(response);
        
        // Update subscription metrics
        metrics_->setActiveSubscriptions(getTotalSubscriptions());
        
        std::cout << "Sent SUBACK" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling SUBSCRIBE: " << e.what() << std::endl;
    }
}

void MqttBroker::handleUnsubscribe(std::shared_ptr<Connection> client, const MqttPacket& packet) {
    std::cout << "Handling UNSUBSCRIBE packet" << std::endl;
    
    try {
        // Parse UNSUBSCRIBE packet
        UnsubscribePacket unsubscribe = UnsubscribePacket::parse(packet);
        
        std::vector<uint8_t> reason_codes;
        
        for (const auto& topic : unsubscribe.topic_filters) {
            std::cout << "Unsubscribe from topic: " << topic << std::endl;
            
            // Remove client from subscription list
            if (subscriptions.find(topic) != subscriptions.end()) {
                auto& subscribers = subscriptions[topic];
                subscribers.erase(
                    std::remove(subscribers.begin(), subscribers.end(), client),
                    subscribers.end()
                );
                
                // Clean up empty subscription lists
                if (subscribers.empty()) {
                    subscriptions.erase(topic);
                }
                
                reason_codes.push_back(0);  // Success
            } else {
                reason_codes.push_back(0x11);  // No subscription existed
            }
        }
        
        // Send UNSUBACK
        MqttPacket unsuback = PacketFactory::create_unsuback(unsubscribe.packet_identifier, reason_codes);
        std::vector<uint8_t> response = unsuback.serialize();
        client->send(response);
        
        // Update subscription metrics
        metrics_->setActiveSubscriptions(getTotalSubscriptions());
        
        std::cout << "Sent UNSUBACK" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling UNSUBSCRIBE: " << e.what() << std::endl;
    }
}

void MqttBroker::handlePingreq(std::shared_ptr<Connection> client) {
    
    MqttPacket pingresp = PacketFactory::create_pingresp();
    std::vector<uint8_t> response = pingresp.serialize();
    client->send(response);
    
    std::cout << "Sent PINGRESP" << std::endl;
}

void MqttBroker::handleDisconnect(std::shared_ptr<Connection> client) {
    std::cout << "Handling DISCONNECT packet (graceful disconnect)" << std::endl;
    cleanupClientSubscriptions(client);
    client->disconnect();
}

void MqttBroker::cleanupClientSubscriptions(std::shared_ptr<Connection> client) {
    // Remove client from all subscriptions
    for (auto& [topic, subscribers] : subscriptions) { 
        subscribers.erase(
            std::remove(subscribers.begin(), subscribers.end(), client),
            subscribers.end()
        );
    }
    
    // Clean up empty subscription lists
    for (auto it = subscriptions.begin(); it != subscriptions.end();) {
        if (it->second.empty()) {
            it = subscriptions.erase(it);
        } else {
            ++it;
        }
    }
}

size_t MqttBroker::getTotalSubscriptions() const {
    size_t total = 0;
    for (const auto& [topic, subscribers] : subscriptions) {
        total += subscribers.size();
    }
    return total;
}

} // namespace mqtt