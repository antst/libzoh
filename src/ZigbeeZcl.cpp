#include "ZigbeeZcl.h"
#include <iostream>
#include <algorithm>

ZigbeeZcl::ZigbeeZcl(ZigbeeAps &aps)
        : m_aps(aps) {
    // Hook APS callback for non-ZDO clusters
    m_aps.setApsDataIndicationCallback(
            [this](uint16_t srcAddr, uint16_t clusterId, const std::vector<uint8_t> &apsData) {
                // If clusterId < 0x0004 or > 0x003F => ZCL
                if (clusterId >= 0x0004 && clusterId < 0x8000) {
                    handleApsFrame(srcAddr, clusterId, apsData);
                }
                // else might be ZDO or Mfg specific
            }
    );
}

ZigbeeZcl::~ZigbeeZcl() {
}

void ZigbeeZcl::registerEndpoint(const ZclEndpoint &ep) {
    m_endpoints.push_back(ep);
}

bool ZigbeeZcl::readAttribute(uint8_t endpointId, uint16_t clusterId, uint16_t attrId,
                              std::vector<uint8_t> &outValue) {
    for (auto &ep: m_endpoints) {
        if (ep.endpointId == endpointId) {
            // find cluster
            for (auto &cl: ep.clusters) {
                if (cl.clusterId == clusterId) {
                    auto it = cl.attributes.find(attrId);
                    if (it != cl.attributes.end()) {
                        outValue = it->second.value;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool ZigbeeZcl::writeAttribute(uint8_t endpointId, uint16_t clusterId, uint16_t attrId,
                               const std::vector<uint8_t> &value) {
    for (auto &ep: m_endpoints) {
        if (ep.endpointId == endpointId) {
            for (auto &cl: ep.clusters) {
                if (cl.clusterId == clusterId) {
                    auto it = cl.attributes.find(attrId);
                    if (it != cl.attributes.end() && it->second.writable) {
                        it->second.value = value;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void ZigbeeZcl::handleApsFrame(uint16_t srcAddr,
                               uint16_t clusterId,
                               const std::vector<uint8_t> &apsPayload) {
    if (apsPayload.size() < 3) {
        std::cerr << "[ZCL] Frame too short\n";
        return;
    }
    // parse basic ZCL header
    uint8_t frameCtrl, seqNo, cmdId;
    std::vector<uint8_t> zclPayload;
    parseZclFrame(clusterId, apsPayload, frameCtrl, seqNo, cmdId, zclPayload);

    // Find endpoint/cluster that matches clusterId
    // For demonstration, we might not know the endpoint from the APS layer (we only got clusterId).
    // Real code might keep track of which endpoint was addressed or we store a “default endpoint” for cluster.
    // We'll just pick the first cluster that matches clusterId
    for (auto &ep: m_endpoints) {
        for (auto &cl: ep.clusters) {
            if (cl.clusterId == clusterId && cl.isServer) {
                // If it's a standard ZCL command, handle read/write attributes
                // If it's a cluster-specific command, call cl.commandHandler
                // Example:
                if ((frameCtrl & 0x08) == 0) // bit for “cluster-specific”
                {
                    // Standard ZCL command: read/write attribute
                    if (cmdId == 0x00) // Read Attributes
                    {
                        // parse a list of attribute IDs, respond with values
                        // ...
                    } else if (cmdId == 0x02) // Write Attributes
                    {
                        // parse each attribute record
                        // ...
                    }
                } else {
                    // cluster-specific
                    if (cl.commandHandler) {
                        cl.commandHandler(cmdId, zclPayload);
                    }
                }
                return;
            }
        }
    }
    std::cout << "[ZCL] No matching cluster for 0x" << std::hex << clusterId << "\n";
}

void ZigbeeZcl::parseZclFrame(uint16_t clusterId,
                              const std::vector<uint8_t> &zclData,
                              uint8_t &frameControl,
                              uint8_t &seqNo,
                              uint8_t &commandId,
                              std::vector<uint8_t> &payload) const {
    frameControl = zclData[0];
    seqNo = zclData[1];
    commandId = zclData[2];
    // remainder is payload
    payload.assign(zclData.begin() + 3, zclData.end());
}
