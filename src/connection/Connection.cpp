#include "Connection.h"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>

Connection::Connection(int socket) : socket_(socket), connected_(true) {}

Connection::~Connection() {
    disconnect();
}

void Connection::disconnect() {
    if (connected_ && socket_ >= 0) {
        close(socket_);
        socket_ = -1;
        connected_ = false;
    }
}

std::vector<uint8_t> Connection::receive() {
    std::vector<uint8_t> buffer(4096);
    
    ssize_t bytesRead = recv(socket_, buffer.data(), buffer.size(), 0);
    
    if (bytesRead <= 0) {
        // Connection closed or error
        connected_ = false;
        return std::vector<uint8_t>();
    }
    
    buffer.resize(bytesRead);
    return buffer;
}

void Connection::send(const std::vector<uint8_t>& data) {
    if (!connected_ || socket_ < 0) {
        return;
    }
    
    ssize_t bytesSent = ::send(socket_, data.data(), data.size(), 0);
    
    if (bytesSent < 0) {
        std::cerr << "Failed to send data" << std::endl;
        connected_ = false;
    }
}