#ifndef MQTT_BROKER_H
#define MQTT_BROKER_H

#include <vector>
#include <memory>
#include <map>
#include "../connection/Connection.h"
#include "../protocol/MqttPacket.h"

namespace mqtt {

class MqttBroker {
public:
    MqttBroker();
    ~MqttBroker();
    
    void start();
    void stop();
    void run();  // Main event loop
    
private:
    int serverSocket;
    bool running;
    std::vector<std::shared_ptr<Connection>> clients;
    
    void acceptNewConnection();
    void handleClientData(std::shared_ptr<Connection> client);
    
    // MQTT packet handlers
    void handleConnect(std::shared_ptr<Connection> client, const MqttPacket& packet);
    void handlePublish(std::shared_ptr<Connection> client, const MqttPacket& packet);
    void handleSubscribe(std::shared_ptr<Connection> client, const MqttPacket& packet);
    void handleUnsubscribe(std::shared_ptr<Connection> client, const MqttPacket& packet);
    void handlePingreq(std::shared_ptr<Connection> client);
    void handleDisconnect(std::shared_ptr<Connection> client);
    
    // Topic management
    std::map<std::string, std::vector<std::shared_ptr<Connection>>> subscriptions;  // topic -> clients
    std::map<std::string, std::pair<std::vector<uint8_t>, uint8_t>> retainedMessages;  // topic -> (message, qos)
};

} // namespace mqtt

#endif // MQTT_BROKER_H