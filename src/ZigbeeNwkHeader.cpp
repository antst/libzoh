#include "ZigbeeNwkHeader.h"
#include <iostream>

// Helper: extract bits from a byte
static inline uint8_t extractBits(uint8_t byte, uint8_t pos, uint8_t len) {
    return (byte >> pos) & ((1 << len) - 1);
}

std::optional<NwkFrameHeader> NwkFrameHeader::parse(const std::vector<uint8_t>& data,
                           size_t offset,
                           size_t &totalHeaderLength)
{
    //FIXME: Should handle more options, like IEEE addresses
    // Ensure there are at least 7 bytes available for the fixed header.
    if (data.size() < offset + 7) {
        std::cerr << "[NWK Parser] Insufficient data for fixed NWK header\n";
        return std::nullopt;
    }

    NwkFrameHeader hdr;
    // --- Parse fixed header (7 bytes) ---
    hdr.frameControl = data[offset];
    hdr.protocolVersion = extractBits(hdr.frameControl, 0, 2);    // bits 0-1
    hdr.frameType = extractBits(hdr.frameControl, 2, 2);          // bits 2-3
    hdr.discoverRoute = ((hdr.frameControl >> 4) & 0x01) != 0;    // bit 4
    hdr.multicast = ((hdr.frameControl >> 5) & 0x01) != 0;          // bit 5
    hdr.security = ((hdr.frameControl >> 6) & 0x01) != 0;           // bit 6
    hdr.sourceRoute = ((hdr.frameControl >> 7) & 0x01) != 0;        // bit 7

    hdr.sequenceNumber = data[offset + 1];
    hdr.dstAddr = data[offset + 2] | (data[offset + 3] << 8);
    hdr.srcAddr = data[offset + 4] | (data[offset + 5] << 8);
    hdr.radius = data[offset + 6];

    // Start with fixed header length
    totalHeaderLength = 7;
    size_t currentOffset = offset + 7;

    // --- Optional: Source Route Record ---
    if (hdr.sourceRoute) {
        // Ensure at least one byte is available for the source route count.
        if (data.size() < currentOffset + 1) {
            std::cerr << "[NWK Parser] Expected source route count byte\n";
            return std::nullopt;
        }
        uint8_t routeCount = data[currentOffset];
        currentOffset += 1;
        totalHeaderLength += 1;

        // Ensure enough bytes for the full source route record
        if (data.size() < currentOffset + routeCount * 2) {
            std::cerr << "[NWK Parser] Insufficient data for source route record\n";
            return std::nullopt;
        }
        hdr.sourceRouteRecord.clear();
        for (uint8_t i = 0; i < routeCount; i++) {
            uint16_t hop = data[currentOffset] | (data[currentOffset + 1] << 8);
            hdr.sourceRouteRecord.push_back(hop);
            currentOffset += 2;
            totalHeaderLength += 2;
        }
    }

    // --- Optional: Auxiliary Security Header ---
    hdr.auxSecPresent = false;
    if (hdr.security) {
        // The Auxiliary Security Header is defined as:
        //   1 byte: Security Control Field
        //   4 bytes: Frame Counter (little-endian)
        //   Optionally, if Key Identifier Mode is not 0, additional bytes follow.
        if (data.size() < currentOffset + 5) {
            std::cerr << "[NWK Parser] Insufficient data for Auxiliary Security Header\n";
            return std::nullopt;
        }
        hdr.auxSecPresent = true;
        hdr.auxSec.securityControl = data[currentOffset];
        currentOffset += 1;
        totalHeaderLength += 1;

        hdr.auxSec.frameCounter = data[currentOffset] |
                                  (data[currentOffset + 1] << 8) |
                                  (data[currentOffset + 2] << 16) |
                                  (data[currentOffset + 3] << 24);
        currentOffset += 4;
        totalHeaderLength += 4;

        // Determine if a key identifier is present.
        // In Zigbee Pro, the Key Identifier Mode field is in bits 2–3 of the Security Control.
        uint8_t keyIdMode = extractBits(hdr.auxSec.securityControl, 2, 2);
        if (keyIdMode != 0) {
            // For our example, assume if keyIdMode is nonzero then one additional byte is present.
            if (data.size() < currentOffset + 1) {
                std::cerr << "[NWK Parser] Expected key identifier byte\n";
                return std::nullopt;
            }
            hdr.auxSec.keyIdentifierPresent = true;
            hdr.auxSec.keyIdentifier = data[currentOffset];
            currentOffset += 1;
            totalHeaderLength += 1;
        } else {
            hdr.auxSec.keyIdentifierPresent = false;
        }
    }

    return hdr;
}

std::vector<uint8_t> NwkFrameHeader::build() const {
    //FIXME: Basic version so far
    std::vector<uint8_t> buf;
    buf.reserve(7);

    // NWK Frame Control: 1 byte
    buf.push_back(this->frameControl);

    // NWK Sequence Number: 1 byte
    buf.push_back(this->sequenceNumber);

    // NWK Destination Address: 2 bytes (little-endian)
    buf.push_back(static_cast<uint8_t>(this->dstAddr & 0xFF));
    buf.push_back(static_cast<uint8_t>((this->dstAddr >> 8) & 0xFF));

    // NWK Source Address: 2 bytes (little-endian)
    buf.push_back(static_cast<uint8_t>(this->srcAddr & 0xFF));
    buf.push_back(static_cast<uint8_t>((this->srcAddr >> 8) & 0xFF));

    // Radius: 1 byte
    buf.push_back(this->radius);

    return buf;
}