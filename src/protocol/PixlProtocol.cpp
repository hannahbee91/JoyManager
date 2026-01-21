#include "PixlProtocol.h"
#include <cstring>
#include <stdexcept>

namespace Pixl {

std::vector<uint8_t> Protocol::createPacket(Command cmd, const std::vector<uint8_t>& payload, uint16_t chunk) {
    std::vector<uint8_t> packet;
    packet.reserve(4 + payload.size());

    packet.push_back(static_cast<uint8_t>(cmd));
    packet.push_back(0); // Status is 0 for requests usually
    packet.push_back(chunk & 0xFF);
    packet.push_back((chunk >> 8) & 0xFF);

    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

Packet Protocol::parsePacket(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        throw std::runtime_error("Packet too short");
    }

    Packet pkt;
    pkt.cmd = data[0];
    pkt.status = data[1];
    pkt.chunk = data[2] | (data[3] << 8);
    
    if (data.size() > 4) {
        pkt.payload.assign(data.begin() + 4, data.end());
    }

    return pkt;
}

std::vector<uint8_t> Protocol::createStringPayload(const std::string& str) {
    std::vector<uint8_t> payload;
    uint16_t len = str.length();
    payload.push_back(len & 0xFF);
    payload.push_back((len >> 8) & 0xFF);
    payload.insert(payload.end(), str.begin(), str.end());
    return payload;
}

std::vector<uint8_t> Protocol::createOpenFilePayload(const std::string& path, uint8_t mode) {
    std::vector<uint8_t> payload = createStringPayload(path);
    payload.push_back(mode);
    return payload;
}

std::vector<uint8_t> Protocol::createRenamePayload(const std::string& oldPath, const std::string& newPath) {
    std::vector<uint8_t> payload = createStringPayload(oldPath);
    std::vector<uint8_t> newPathPayload = createStringPayload(newPath);
    payload.insert(payload.end(), newPathPayload.begin(), newPathPayload.end());
    return payload;
}

std::string Protocol::parseString(const std::vector<uint8_t>& payload, size_t& offset) {
    if (offset + 2 > payload.size()) return "";
    uint16_t len = payload[offset] | (payload[offset + 1] << 8);
    offset += 2;
    if (offset + len > payload.size()) return "";
    
    std::string str(payload.begin() + offset, payload.begin() + offset + len);
    offset += len;
    return str;
}

uint16_t Protocol::parseUInt16(const std::vector<uint8_t>& payload, size_t& offset) {
    if (offset + 2 > payload.size()) return 0;
    uint16_t val = payload[offset] | (payload[offset + 1] << 8);
    offset += 2;
    return val;
}

uint32_t Protocol::parseUInt32(const std::vector<uint8_t>& payload, size_t& offset) {
    if (offset + 4 > payload.size()) return 0;
    uint32_t val = payload[offset] | (payload[offset + 1] << 8) | 
                   (payload[offset + 2] << 16) | (payload[offset + 3] << 24);
    offset += 4;
    return val;
}

} // namespace Pixl
