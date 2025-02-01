#include "ZigbeeMacHeader.h"
#include <cstddef>

std::optional<MacFrameHeader> MacFrameHeader::parse(const std::vector<uint8_t>& data) {
    constexpr size_t kRequiredSize = 9; // 2 + 1 + 2 + 2 + 2
    if (data.size() < kRequiredSize) {
        // Not enough data to parse the MAC header.
        return std::nullopt;
    }

    MacFrameHeader header;
    // Frame Control: 2 bytes, little-endian.
    header.frameControl = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
    // Sequence Number: byte 2.
    header.sequenceNumber = data[2];
    // Destination PAN ID: bytes 3-4, little-endian.
    header.destPanId = static_cast<uint16_t>(data[3]) | (static_cast<uint16_t>(data[4]) << 8);
    // Destination Address: bytes 5-6, little-endian.
    header.destAddr = static_cast<uint16_t>(data[5]) | (static_cast<uint16_t>(data[6]) << 8);
    // Source Address: bytes 7-8, little-endian.
    header.srcAddr = static_cast<uint16_t>(data[7]) | (static_cast<uint16_t>(data[8]) << 8);

    return header;
}

std::vector<uint8_t> MacFrameHeader::build() const {
    std::vector<uint8_t> buf;
    buf.reserve(9);

    // Frame Control: 2 bytes (little-endian)
    buf.push_back(static_cast<uint8_t>(this->frameControl & 0xFF));
    buf.push_back(static_cast<uint8_t>((this->frameControl >> 8) & 0xFF));

    // Sequence Number: 1 byte
    buf.push_back(this->sequenceNumber);

    // Destination PAN ID: 2 bytes (little-endian)
    buf.push_back(static_cast<uint8_t>(this->destPanId & 0xFF));
    buf.push_back(static_cast<uint8_t>((this->destPanId >> 8) & 0xFF));

    // Destination Address: 2 bytes (little-endian)
    buf.push_back(static_cast<uint8_t>(this->destAddr & 0xFF));
    buf.push_back(static_cast<uint8_t>((this->destAddr >> 8) & 0xFF));

    // Source Address: 2 bytes (little-endian)
    buf.push_back(static_cast<uint8_t>(this->srcAddr & 0xFF));
    buf.push_back(static_cast<uint8_t>((this->srcAddr >> 8) & 0xFF));

    return buf;
}