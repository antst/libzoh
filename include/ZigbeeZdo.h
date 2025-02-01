#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "ZigbeeAps.h"
#include "ZdoEndpointTable.h"
#include "ZdoStructs.h"

class ZigbeeZdo {
public:
    ZigbeeZdo(ZigbeeAps &aps,
              uint16_t nwkAddr,
              uint64_t extAddr);

    ~ZigbeeZdo();

    void start();

    // Called by APS for frames on endpoint=0 with cluster in [0x0000..0x003F]
    void handleZdoFrame(uint16_t srcAddr,
                        uint16_t clusterId,
                        const std::vector<uint8_t> &zdoPayload);

    // Basic descriptors
    void setNodeDescriptor(const NodeDescriptor &nodeDesc);

    void setPowerDescriptor(const PowerDescriptor &powerDesc);

    // Additional descriptors:
    void setUserDescriptor(const UserDescriptor &userDesc);

    void setComplexDescriptor(const ComplexDescriptor &complexDesc);

    // The local endpoint table
    ZdoEndpointTable &endpoints();

    // DeviceAnnce helper
    bool sendDeviceAnnounce(uint16_t dstAddr);

private:
    // Request handlers
    void handleNetworkAddrReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleIeeeAddrReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleNodeDescReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handlePowerDescReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleSimpleDescReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleActiveEPReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleMatchDescReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    // Bind/Unbind
    void handleBindReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleUnbindReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    // Mgmt commands
    void handleMgmtLqiReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleMgmtLeaveReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleMgmtPermitJoinReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleMgmtNwkUpdateReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    // User / Complex descriptor
    void handleUserDescReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleUserDescSet(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    void handleComplexDescReq(uint16_t srcAddr, const std::vector<uint8_t> &payload);

    // Helpers to build responses
    bool sendZdoResponse(uint16_t dstAddr, uint16_t clusterId, const std::vector<uint8_t> &payload);

private:
    ZigbeeAps &m_aps;

    uint16_t m_nwkAddr;
    uint64_t m_extAddr;

    NodeDescriptor m_nodeDesc;
    PowerDescriptor m_powerDesc;
    UserDescriptor m_userDesc;
    ComplexDescriptor m_complexDesc;

    ZdoEndpointTable m_endpointTable;
};
