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
    PacketType packet_type;
    bool dupe {false};
    QoSLevel qos {QoSLevel::AT_MOST_ONCE};
    bool retain {false};
};


class MqttPacket {
public:
    MqttPacket();
    ~MqttPacket();

    // Setters, builder style
    MqttPacket& set_header(const Header& h);
    MqttPacket& set_payload(const std::vector<uint8_t>& data);

    // Getters
    const Header& get_header() const;
    PacketType get_packet_type() const;
    const std::vector<uint8_t>& get_payload() const;
    uint32_t get_remaining_length() const;
    
    // Flag helpers
    bool get_dup_flag() const;
    QoSLevel get_qos() const;
    bool get_retain_flag() const;

    // Parsing and serialization
    std::vector<uint8_t> serialize() const;
    static MqttPacket parse(const std::vector<uint8_t>& buffer);

    // Helper methods for reading from payload
    static uint16_t read_uint16(const std::vector<uint8_t>& data, size_t& index);
    static std::string read_utf8_string(const std::vector<uint8_t>& data, size_t& index);
    static uint8_t read_byte(const std::vector<uint8_t>& data, size_t& index);
    static uint32_t read_variable_byte_integer(const std::vector<uint8_t>& data, size_t& index);
    
    // Helper methods for writing to payload
    static void write_uint16(std::vector<uint8_t>& data, uint16_t value);
    static void write_utf8_string(std::vector<uint8_t>& data, const std::string& str);
    static void write_byte(std::vector<uint8_t>& data, uint8_t value);
    static void write_variable_byte_integer(std::vector<uint8_t>& data, uint32_t value);

private:
    static Header decode_header(const std::vector<uint8_t>& buffer, size_t& index);
    static uint32_t decode_remaining_length(const std::vector<uint8_t>& buffer, size_t& index);
    static std::vector<uint8_t> encode_header(const Header& header);
    static std::vector<uint8_t> encode_remaining_length(uint32_t length);

    Header header {};
    std::vector<uint8_t> payload {};  // Variable header + payload combined
};

// Packet-specific parsing helper structures
struct ConnectPacket {
    std::string protocol_name;
    uint8_t protocol_version;
    uint8_t connect_flags;
    uint16_t keep_alive;
    std::string client_id;
    std::string will_topic;
    std::string will_message;
    std::string username;
    std::string password;
    std::map<uint8_t, std::vector<uint8_t>> properties;
    
    static ConnectPacket parse(const MqttPacket& packet);
};

struct PublishPacket {
    std::string topic_name;
    uint16_t packet_identifier;  // Only for QoS > 0
    std::vector<uint8_t> message;
    std::map<uint8_t, std::vector<uint8_t>> properties;
    
    static PublishPacket parse(const MqttPacket& packet);
};

struct SubscribePacket {
    uint16_t packet_identifier;
    std::vector<std::pair<std::string, uint8_t>> topic_filters;  // topic, qos
    std::map<uint8_t, std::vector<uint8_t>> properties;
    
    static SubscribePacket parse(const MqttPacket& packet);
};

struct UnsubscribePacket {
    uint16_t packet_identifier;
    std::vector<std::string> topic_filters;
    std::map<uint8_t, std::vector<uint8_t>> properties;
    
    static UnsubscribePacket parse(const MqttPacket& packet);
};

// Helper functions for creating response packets
namespace PacketFactory {
    MqttPacket create_connack(uint8_t session_present, uint8_t reason_code);
    MqttPacket create_publish(const std::string& topic, const std::vector<uint8_t>& message, 
                              QoSLevel qos, bool retain, uint16_t packet_id = 0);
    MqttPacket create_puback(uint16_t packet_identifier, uint8_t reason_code = 0);
    MqttPacket create_suback(uint16_t packet_identifier, const std::vector<uint8_t>& reason_codes);
    MqttPacket create_unsuback(uint16_t packet_identifier, const std::vector<uint8_t>& reason_codes);
    MqttPacket create_pingresp();
    MqttPacket create_disconnect(uint8_t reason_code = 0);
}

} // namespace mqtt

#endif // MQTTPACKET_H