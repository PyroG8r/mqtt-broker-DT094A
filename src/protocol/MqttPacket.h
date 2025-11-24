#ifndef MQTTPACKET_H
#define MQTTPACKET_H

#include <cstdint>
#include <vector>

class MqttPacket {
public:
    MqttPacket();
    ~MqttPacket();

private:
    uint8_t packetType;
    std::vector<uint8_t> packetPayload;
};

#endif // MQTTPACKET_H