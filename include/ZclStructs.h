#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>

/**
 * Basic representation of an attribute
 */
struct ZclAttribute
{
    uint16_t attrId;
    uint8_t  dataType; // e.g. 0x10 = boolean, 0x20=uint8, etc.
    bool     writable;
    std::vector<uint8_t> value; // raw bytes
};

/**
 * Single cluster instance
 */
struct ZclCluster
{
    uint16_t clusterId;
    bool isServer;
    // attributeId => ZclAttribute
    std::unordered_map<uint16_t, ZclAttribute> attributes;

    // Callback for cluster-specific commands
    std::function<void(uint16_t commandId,
                       const std::vector<uint8_t> &payload)> commandHandler;
};

/**
 * Single endpoint, referencing a set of clusters
 */
struct ZclEndpoint
{
    uint8_t endpointId;
    uint16_t profileId;
    std::vector<ZclCluster> clusters;
};
