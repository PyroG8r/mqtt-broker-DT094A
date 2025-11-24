#ifndef MQTTPACKET_H
#define MQTTPACKET_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace mqtt {

enum class PacketType : uint8_t {
    CONNECT     = 1,
    CONNACK     = 2,
    PUBLISH     = 3,
    PUBACK      = 4,
    PUBREC      = 5,
    PUBREL      = 6,
    PUBCOMP     = 7,
    SUBSCRIBE   = 8,
    SUBACK      = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK    = 11,
    PINGREQ     = 12,
    PINGRESP    = 13,
    DISCONNECT  = 14,
    AUTH        = 15
};

enum class QoSLevel : uint8_t {
    AT_MOST_ONCE  = 0,
    AT_LEAST_ONCE = 1,
    EXACTLY_ONCE  = 2
};

struct Header {
    PacketType packetType;
    uint8_t flags;  // Bits 3-0: DUP, QoS (2 bits), RETAIN
    uint32_t remainingLength; // Variable length encoding
};


class MqttPacket {
public:
    MqttPacket();
    ~MqttPacket();

    // Setters, builder style
    MqttPacket& set_packet_type(PacketType type);
    MqttPacket& set_flags(uint8_t flags);
    MqttPacket& set_packet_payload(std::vector<uint8_t> payload);

    // Getters
    const Header& get_header() const;
    PacketType get_packet_type() const;
    const std::vector<uint8_t>& get_packet_payload() const;
    
    // Flag helpers (for PUBLISH packets)
    bool get_dup_flag() const;
    QoSLevel get_qos() const;
    bool get_retain_flag() const;
    
    // Variable header field accessors (packet-specific)
    std::string get_client_id() const;
    std::string get_topic() const;
    uint16_t get_packet_identifier() const;
    uint8_t get_return_code() const;
    
    // Property accessors
    const std::map<uint8_t, std::vector<uint8_t>>& get_properties() const;
    void add_property(uint8_t id, const std::vector<uint8_t>& value);

    // Serialization
    std::vector<uint8_t> serialize() const;
    static MqttPacket parse(const std::vector<uint8_t>& buffer);

private:
    static std::vector<uint8_t> encode_header(const Header& header);
    static Header decode_header(const std::vector<uint8_t>& buffer, size_t& index);
    static uint32_t decode_remaining_length(const std::vector<uint8_t>& buffer, size_t& index);
    static std::vector<uint8_t> encode_remaining_length(uint32_t length);

    Header header {};
    std::vector<uint8_t> packet_payload {};
    
    // Variable header fields (populated during parsing based on packet type)
    std::string client_id;
    std::string topic;
    uint16_t packet_identifier {0};
    uint8_t return_code {0};
    
    // MQTT 5.0 properties
    std::map<uint8_t, std::vector<uint8_t>> properties;
};

} // namespace mqtt

#endif // MQTTPACKET_H