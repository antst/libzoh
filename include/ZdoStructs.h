#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

// A user descriptor is up to 16 bytes typically (Zigbee limit).
struct UserDescriptor
{
    std::string userDesc; // e.g. "KitchenLight"
};

// Complex descriptor might hold device-specific info, often rarely used.
struct ComplexDescriptor
{
    // Possibly a manufacturer name, URL, icon data, etc.
    // We’ll store a placeholder string.
    std::string info;
};

struct NodeDescriptor
{
    // e.g. bits for device type, complex descriptor, user descriptor, etc.
    uint8_t type;          // 0=Coordinator, 1=Router, 2=EndDevice
    uint16_t manufacturerCode;
    uint8_t capabilityFlags; // e.g. 'rxOnWhenIdle', 'security', etc.
    // etc. for 'maximum buffer size', 'maximum transfer size', etc.
};

struct PowerDescriptor
{
    uint8_t currentPowerMode;
    uint8_t availablePowerSources;
    uint8_t currentPowerSource;
    uint8_t currentPowerSourceLevel;
};
/**
 * This struct describes the “Simple Descriptor” of a single endpoint:
 * which clusters it supports as server/client, etc.
 */
struct SimpleDescriptor
{
    uint8_t  endpoint;
    uint16_t profileId;
    // Lists of server and client cluster IDs
    std::vector<uint16_t> inClusterList;
    std::vector<uint16_t> outClusterList;
};