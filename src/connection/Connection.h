#ifndef CONNECTION_H
#define CONNECTION_H

#include <vector>
#include <cstdint>

namespace mqtt {

class Connection {
public:
    Connection(int socket);
    ~Connection();
    
    void disconnect();
    std::vector<uint8_t> receive();
    void send(const std::vector<uint8_t>& data);
    
    int getSocket() const { return socket_; }
    bool isConnected() const { return connected_; }
    
private:
    int socket_;
    bool connected_;
};

} // namespace mqtt

#endif // CONNECTION_H