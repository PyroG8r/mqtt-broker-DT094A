#ifndef MQTT_BROKER_H
#define MQTT_BROKER_H

#include <vector>
#include <memory>
#include "../connection/Connection.h"

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
};

} // namespace mqtt

#endif // MQTT_BROKER_H