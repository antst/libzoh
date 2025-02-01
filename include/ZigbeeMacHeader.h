#pragma once

#include <cstdint>
#include <vector>
#include <optional>

/**
 * @brief Structure representing a parsed 802.15.4 MAC header.
 *
 * For our fixed-format example (beaconless network with 16-bit addresses):
 * - Frame Control: 2 bytes (little-endian)
 * - Sequence Number: 1 byte
 * - Destination PAN ID: 2 bytes (little-endian)
 * - Destination Address: 2 bytes (16-bit short address, little-endian)
 * - Source Address: 2 bytes (16-bit short address, little-endian)
 */
class MacFrameHeader {
public:
    static std::optional<MacFrameHeader> parse(const std::vector<uint8_t>& data);
/**
 * @brief Builds a MAC header as a byte buffer from a given MacFrameHeader structure.
 *
 * @param header The MacFrameHeader structure with the fields to send.
 * @return A vector of bytes containing the MAC header.
 */
    std::vector<uint8_t> build() const;

    uint16_t frameControl;
    uint8_t  sequenceNumber;
    uint16_t destPanId;
    uint16_t destAddr;
    uint16_t srcAddr;

};




