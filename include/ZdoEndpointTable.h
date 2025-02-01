#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "ZdoStructs.h"


/**
 * ZdoEndpointTable holds known endpoints for this device.
 */
class ZdoEndpointTable
{
public:
    void addSimpleDescriptor(const SimpleDescriptor &desc);
    bool getSimpleDescriptor(uint8_t endpoint, SimpleDescriptor &outDesc) const;
    std::vector<uint8_t> getActiveEndpoints() const;

private:
    std::unordered_map<uint8_t, SimpleDescriptor> m_epMap;
};
