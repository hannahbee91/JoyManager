#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace Pixl {

// UUIDs
const char* const SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char* const RX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // Host writes to this (TX from Host perspective, RX for Device)
const char* const TX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // Device notifies this (RX from Host perspective)

enum class Command : uint8_t {
    GetVersion = 0x01,
    EnterDfu = 0x02,
    GetDriveList = 0x10,
    DriveFormat = 0x11,
    OpenFile = 0x12,
    CloseFile = 0x13,
    ReadFile = 0x14,
    WriteFile = 0x15,
    ReadDir = 0x16,
    CreateFolder = 0x17,
    Remove = 0x18,
    Rename = 0x19,
    UpdateMeta = 0x1A
};

struct Packet {
    uint8_t cmd;
    uint8_t status;
    uint16_t chunk; // Includes the more_data flag in MSB
    std::vector<uint8_t> payload;

    bool hasMoreData() const { return (chunk & 0x8000) != 0; }
    uint16_t chunkIndex() const { return chunk & 0x7FFF; }
};

class Protocol {
public:
    static std::vector<uint8_t> createPacket(Command cmd, const std::vector<uint8_t>& payload = {}, uint16_t chunk = 0);
    static Packet parsePacket(const std::vector<uint8_t>& data);

    // Helpers for payload creation
    static std::vector<uint8_t> createStringPayload(const std::string& str);
    static std::vector<uint8_t> createOpenFilePayload(const std::string& path, uint8_t mode);
    static std::vector<uint8_t> createRenamePayload(const std::string& oldPath, const std::string& newPath);
    
    // Helpers for payload parsing
    static std::string parseString(const std::vector<uint8_t>& payload, size_t& offset);
    static uint16_t parseUInt16(const std::vector<uint8_t>& payload, size_t& offset);
    static uint32_t parseUInt32(const std::vector<uint8_t>& payload, size_t& offset);
};

struct FileEntry {
    std::string name;
    uint32_t size;
    uint8_t type; // 1 = dir, 0 = file
    std::string meta;
};

} // namespace Pixl
