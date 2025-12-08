#include "MqttPacket.h"
#include <stdexcept>
#include <cstring>

namespace mqtt {

// ============================================================================
// MqttPacket Implementation
// ============================================================================

MqttPacket::MqttPacket() {}

MqttPacket::~MqttPacket() {}

// Setters
MqttPacket& MqttPacket::set_header(const Header& h) {
    header = h;
    return *this;
}

MqttPacket& MqttPacket::set_payload(const std::vector<uint8_t>& data) {
    payload = data;
    return *this;
}

// Getters
const Header& MqttPacket::get_header() const {
    return header;
}

PacketType MqttPacket::get_packet_type() const {
    return header.packet_type;
}

const std::vector<uint8_t>& MqttPacket::get_payload() const {
    return payload;
}

uint32_t MqttPacket::get_remaining_length() const {
    return static_cast<uint32_t>(payload.size());
}

// Flag helpers
bool MqttPacket::get_dup_flag() const {
    return header.dupe;
}

QoSLevel MqttPacket::get_qos() const {
    return header.qos;
}

bool MqttPacket::get_retain_flag() const {
    return header.retain;
}

MqttPacket MqttPacket::parse(const std::vector<uint8_t>& buffer) {
    MqttPacket packet;
    size_t index = 0;
    
    // Decode fixed header
    packet.header = decode_header(buffer, index);
    
    // Decode remaining length
    uint32_t remaining_length = decode_remaining_length(buffer, index);
    
    packet.payload.assign(buffer.begin() + index, buffer.begin() + index + remaining_length);
    
    return packet;
}


Header MqttPacket::decode_header(const std::vector<uint8_t>& buffer, size_t& index) {
    if (index >= buffer.size()) {
        throw std::runtime_error("Buffer too small for header");
    }
    
    Header header;
    uint8_t first_byte = buffer[index++];
    
    header.packet_type = static_cast<PacketType>(first_byte >> 4);
    header.dupe = (first_byte & 0x08) != 0;
    header.qos = static_cast<QoSLevel>((first_byte & 0x06) >> 1);
    header.retain = (first_byte & 0x01) != 0;
    
    return header;
}

uint32_t MqttPacket::decode_remaining_length(const std::vector<uint8_t>& buffer, size_t& index) {
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t encoded_byte;
    
    do {
        if (index >= buffer.size()) {
            throw std::runtime_error("Buffer too small for remaining length");
        }
        
        encoded_byte = buffer[index++];
        value += (encoded_byte & 0x7F) * multiplier;
        
        if (multiplier > 128 * 128 * 128) {
            throw std::runtime_error("Malformed remaining length");
        }
        
        multiplier *= 128;
    } while ((encoded_byte & 0x80) != 0);
    
    return value;
}

std::vector<uint8_t> MqttPacket::serialize() const {
    std::vector<uint8_t> buffer;
    
    // Encode fixed header (first byte)
    uint8_t first_byte = (static_cast<uint8_t>(header.packet_type) << 4) |
                         (header.dupe ? 0x08 : 0x00) |
                         (static_cast<uint8_t>(header.qos) << 1) |
                         (header.retain ? 0x01 : 0x00);
    buffer.push_back(first_byte);
    
    // Encode remaining length
    std::vector<uint8_t> remaining_length_bytes = encode_remaining_length(payload.size());
    buffer.insert(buffer.end(), remaining_length_bytes.begin(), remaining_length_bytes.end());
    
    // Add payload
    buffer.insert(buffer.end(), payload.begin(), payload.end());
    
    return buffer;
}

std::vector<uint8_t> MqttPacket::encode_remaining_length(uint32_t length) {
    std::vector<uint8_t> result;
    
    do {
        uint8_t encoded_byte = length % 128;
        length /= 128;
        
        if (length > 0) {
            encoded_byte |= 0x80;  // Set continuation bit
        }
        
        result.push_back(encoded_byte);
    } while (length > 0 );


    
    return result;
}


uint16_t MqttPacket::read_uint16(const std::vector<uint8_t>& data, size_t& index) {

    uint16_t value = (static_cast<uint16_t>(data[index]) << 8) | data[index + 1];
    index += 2;
    return value;
}

// Read UTF-8 string, reads length prefix first, then the string data
std::string MqttPacket::read_utf8_string(const std::vector<uint8_t>& data, size_t& index) {
    uint16_t length = read_uint16(data, index);

    std::string result(data.begin() + index, data.begin() + index + length);
    index += length;
    return result;
}

uint8_t MqttPacket::read_byte(const std::vector<uint8_t>& data, size_t& index) {
    if (index >= data.size()) {
        throw std::runtime_error("Cannot read byte: buffer too small");
    }
    return data[index++];
}

uint32_t MqttPacket::read_variable_byte_integer(const std::vector<uint8_t>& data, size_t& index) {
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t encoded_byte;
    
    do {
        if (index >= data.size()) {
            throw std::runtime_error("Cannot read variable byte integer: buffer too small");
        }
        
        encoded_byte = data[index++];
        value += (encoded_byte & 0x7F) * multiplier;
        
        if (multiplier > 128 * 128 * 128) {
            throw std::runtime_error("Malformed variable byte integer");
        }
        
        multiplier *= 128;
    } while ((encoded_byte & 0x80) != 0);
    
    return value;
}

void MqttPacket::write_uint16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>(value >> 8));
    data.push_back(static_cast<uint8_t>(value & 0xFF));
}

void MqttPacket::write_utf8_string(std::vector<uint8_t>& data, const std::string& str) {
    write_uint16(data, static_cast<uint16_t>(str.length()));
    data.insert(data.end(), str.begin(), str.end());
}

void MqttPacket::write_byte(std::vector<uint8_t>& data, uint8_t value) {
    data.push_back(value);
}

void MqttPacket::write_variable_byte_integer(std::vector<uint8_t>& data, uint32_t value) {
    do {
        uint8_t encoded_byte = value % 128;
        value /= 128;
        
        if (value > 0) {
            encoded_byte |= 0x80;
        }
        
        data.push_back(encoded_byte);
    } while (value > 0);
}

ConnectPacket ConnectPacket::parse(const MqttPacket& packet) {
    ConnectPacket connect;
    const auto& payload = packet.get_payload();
    size_t index = 0;
    
    connect.protocol_name = MqttPacket::read_utf8_string(payload, index);
    
    connect.protocol_version = MqttPacket::read_byte(payload, index);
    
    connect.connect_flags = MqttPacket::read_byte(payload, index);
    
    connect.keep_alive = MqttPacket::read_uint16(payload, index);
    
    // Properties (MQTT 5.0)
    if (connect.protocol_version == 5) {
        uint32_t prop_length = MqttPacket::read_variable_byte_integer(payload, index);
        size_t prop_end = index + prop_length;
        
        while (index < prop_end) {
            uint8_t prop_id = MqttPacket::read_byte(payload, index);
            // TODO: Parse properties based on prop_id
            // For now, skip property parsing
        }
        index = prop_end;
    }
    
    // Client ID
    connect.client_id = MqttPacket::read_utf8_string(payload, index);
    
    // Will topic and message (if will flag is set)
    if (connect.connect_flags & 0x04) { // Will flag - bit 2
        // Will properties (MQTT 5.0)
        if (connect.protocol_version == 5) {
            uint32_t will_prop_length = MqttPacket::read_variable_byte_integer(payload, index);
            index += will_prop_length;  // Skip for now
        }
        
        connect.will_topic = MqttPacket::read_utf8_string(payload, index);
        connect.will_message = MqttPacket::read_utf8_string(payload, index);
    }
    
    // Username (if username flag is set)
    if (connect.connect_flags & 0x80) { // Username flag - bit 7
        connect.username = MqttPacket::read_utf8_string(payload, index);
    }
    
    // Password (if password flag is set)
    if (connect.connect_flags & 0x40) { // Password flag - bit 6
        connect.password = MqttPacket::read_utf8_string(payload, index);
    }
    
    return connect;
}

PublishPacket PublishPacket::parse(const MqttPacket& packet) {
    PublishPacket publish;
    const auto& payload = packet.get_payload();
    size_t index = 0;
    
    publish.topic_name = MqttPacket::read_utf8_string(payload, index);
    
    // Packet identifier (only if QoS > 0)
    if (packet.get_qos() != QoSLevel::AT_MOST_ONCE) {
        publish.packet_identifier = MqttPacket::read_uint16(payload, index);
    }
    
    uint32_t prop_length = MqttPacket::read_variable_byte_integer(payload, index);
    index += prop_length;  // Skip properties 
    
    publish.message.assign(payload.begin() + index, payload.end());
    
    return publish;
}

SubscribePacket SubscribePacket::parse(const MqttPacket& packet) {
    SubscribePacket subscribe;
    const auto& payload = packet.get_payload();
    size_t index = 0;
    
    // Packet identifier
    subscribe.packet_identifier = MqttPacket::read_uint16(payload, index);
    
    uint32_t prop_length = MqttPacket::read_variable_byte_integer(payload, index);
    index += prop_length;  // Skip properties
    
    // Topic filters
    while (index < payload.size()) {
        std::string topic = MqttPacket::read_utf8_string(payload, index); // What filter   
        uint8_t qos = MqttPacket::read_byte(payload, index); // What QoS
        subscribe.topic_filters.push_back({topic, qos}); //push pair
    }
    
    return subscribe;
}

UnsubscribePacket UnsubscribePacket::parse(const MqttPacket& packet) {
    UnsubscribePacket unsubscribe;
    const auto& payload = packet.get_payload();
    size_t index = 0;
    
    unsubscribe.packet_identifier = MqttPacket::read_uint16(payload, index);
    
    uint32_t prop_length = MqttPacket::read_variable_byte_integer(payload, index);
    index += prop_length;  // Skip properties
    
    // Topic filters
    while (index < payload.size()) {
        std::string topic = MqttPacket::read_utf8_string(payload, index);
        unsubscribe.topic_filters.push_back(topic);
    }
    
    return unsubscribe;
}


namespace PacketFactory {

MqttPacket create_connack(uint8_t session_present, uint8_t reason_code) {
    MqttPacket packet;
    
    Header header;
    header.packet_type = PacketType::CONNACK;
    
    std::vector<uint8_t> payload;
    payload.push_back(session_present & 0x01);  // Connect Acknowledge Flags
    payload.push_back(reason_code);             // Reason Code
    payload.push_back(0);                       // Property Length = 0 (no properties)
    
    packet.set_header(header).set_payload(payload);
    return packet;
}

MqttPacket create_publish(const std::string& topic, const std::vector<uint8_t>& message,
                          QoSLevel qos, bool retain, uint16_t packet_id) {
    MqttPacket packet;
    
    Header header;
    header.packet_type = PacketType::PUBLISH;
    header.dupe = false;
    header.qos = qos;
    header.retain = retain;
    
    std::vector<uint8_t> payload;
    MqttPacket::write_utf8_string(payload, topic);
    
    if (qos != QoSLevel::AT_MOST_ONCE) {
        MqttPacket::write_uint16(payload, packet_id);
    }
    
    payload.push_back(0);  // Property Length = 0
    
    payload.insert(payload.end(), message.begin(), message.end()); // 
    
    packet.set_header(header).set_payload(payload);
    return packet;
}

MqttPacket create_puback(uint16_t packet_identifier, uint8_t reason_code) {
    MqttPacket packet;
    
    Header header;
    header.packet_type = PacketType::PUBACK;
    
    std::vector<uint8_t> payload;
    MqttPacket::write_uint16(payload, packet_identifier);
    payload.push_back(reason_code);
    payload.push_back(0);  // Property Length = 0
    
    packet.set_header(header).set_payload(payload);
    return packet;
}

MqttPacket create_suback(uint16_t packet_identifier, const std::vector<uint8_t>& reason_codes) {
    MqttPacket packet;
    
    Header header;
    header.packet_type = PacketType::SUBACK;
    
    std::vector<uint8_t> payload;
    MqttPacket::write_uint16(payload, packet_identifier);
    payload.push_back(0);  // Property Length = 0
    
    payload.insert(payload.end(), reason_codes.begin(), reason_codes.end());
    
    packet.set_header(header).set_payload(payload);
    return packet;
}

MqttPacket create_unsuback(uint16_t packet_identifier, const std::vector<uint8_t>& reason_codes) {
    MqttPacket packet;
    
    Header header;
    header.packet_type = PacketType::UNSUBACK;
    
    std::vector<uint8_t> payload;
    MqttPacket::write_uint16(payload, packet_identifier);
    payload.push_back(0);  // Property Length = 0
    
    payload.insert(payload.end(), reason_codes.begin(), reason_codes.end());
    
    packet.set_header(header).set_payload(payload);
    return packet;
}

MqttPacket create_pingresp() {
    MqttPacket packet;
    
    Header header;
    header.packet_type = PacketType::PINGRESP;
    
    packet.set_header(header).set_payload({});
    return packet;
}

MqttPacket create_disconnect(uint8_t reason_code) {
    MqttPacket packet;
    
    Header header;
    header.packet_type = PacketType::DISCONNECT;
    
    std::vector<uint8_t> payload;
    payload.push_back(reason_code);
    payload.push_back(0);  // Property Length = 0
    
    packet.set_header(header).set_payload(payload);
    return packet;
}

} // namespace PacketFactory

} // namespace mqtt
