#pragma once
#include <cstdint>
#include <vector>
#include "ZigbeeAps.h"
#include "ZclStructs.h" // Contains ZclEndpoint, ZclCluster, etc.

class ZigbeeZcl
{
public:
    ZigbeeZcl(ZigbeeAps &aps);
    ~ZigbeeZcl();

    // Register an endpoint with clusters
    void registerEndpoint(const ZclEndpoint &ep);

    // Called by APS for inbound frames with clusterId != ZDO range
    void handleApsFrame(uint16_t srcAddr,
                        uint16_t clusterId,
                        const std::vector<uint8_t> &apsPayload);

    // Utility to read attribute
    bool readAttribute(uint8_t endpointId, uint16_t clusterId, uint16_t attrId,
                       std::vector<uint8_t> &outValue);

    // Utility to write attribute
    bool writeAttribute(uint8_t endpointId, uint16_t clusterId, uint16_t attrId,
                        const std::vector<uint8_t> &value);

private:
    ZigbeeAps &m_aps;
    std::vector<ZclEndpoint> m_endpoints;

    // parseZclHeader, dispatch
    void parseZclFrame(uint16_t clusterId, const std::vector<uint8_t> &zclData,
                       uint8_t &frameControl, uint8_t &seqNo, uint8_t &commandId,
                       std::vector<uint8_t> &payload) const;
};
