#pragma once

#include <cstdint>
#include <vector>
#include <optional>

// Auxiliary Security Header structure
struct AuxSecurityHeader {
    uint8_t securityControl;   // 8 bits, contains key identifier mode bits
    uint32_t frameCounter;     // 32-bit frame counter
    bool keyIdentifierPresent; // true if key identifier is present (Key Identifier Mode != 0)
    uint8_t keyIdentifier;     // present only if keyIdentifierPresent is true
};

// Full NWK header structure (including optional fields)
struct NwkFrameHeader {
public:
    static std::optional<NwkFrameHeader> parse(const std::vector<uint8_t>& data,
                      size_t offset,               size_t &totalHeaderLength);

    std::vector<uint8_t> build() const;

    // Basic NWK header fields (first 7 bytes)
    uint8_t  frameControl;       // raw byte
    uint8_t  protocolVersion;    // bits 0-1 of frameControl
    uint8_t  frameType;          // bits 2-3 of frameControl
    bool     discoverRoute;      // bit 4
    bool     multicast;          // bit 5
    bool     security;           // bit 6
    bool     sourceRoute;        // bit 7
    uint8_t  sequenceNumber;     // NWK sequence number
    uint16_t dstAddr;            // NWK Destination Address
    uint16_t srcAddr;            // NWK Source Address
    uint8_t  radius;             // NWK radius

    // Optional Source Route Record (if sourceRoute flag is set)
    std::vector<uint16_t> sourceRouteRecord;  // list of intermediate hops

    // Optional Auxiliary Security Header (if security flag is set)
    bool auxSecPresent;         // should be true if security flag is set
    AuxSecurityHeader auxSec;   // parsed auxiliary security header
};

/**
 * Parse a Zigbee NWK frame header (as per Zigbee 3.0 spec) from a given byte vector.
 *
 * @param data              The full frame buffer (starting at the beginning of the NWK header).
 * @param offset            The offset into data where the NWK header begins.
 * @param hdr               Output: parsed NWK header (including optional fields).
 * @param totalHeaderLength Output: total number of bytes consumed by the NWK header (fixed + optional).
 *
 * @return true if the header was successfully parsed; false otherwise.
 */
