#include "ZigbeeZdo.h"
#include <iostream>
#include <cstring>
#include <algorithm>

ZigbeeZdo::ZigbeeZdo(ZigbeeAps &aps, uint16_t nwkAddr, uint64_t extAddr)
        : m_aps(aps), m_nwkAddr(nwkAddr), m_extAddr(extAddr) {
    // Suppose the APS layer calls handleZdoFrame whenever clusterId < 0x0040 on endpoint=0
}

ZigbeeZdo::~ZigbeeZdo() {
}

void ZigbeeZdo::setNodeDescriptor(const NodeDescriptor &nodeDesc) {
    m_nodeDesc = nodeDesc;
}

void ZigbeeZdo::setPowerDescriptor(const PowerDescriptor &powerDesc) {
    m_powerDesc = powerDesc;
}

ZdoEndpointTable &ZigbeeZdo::endpoints() {
    return m_endpointTable;
}

bool ZigbeeZdo::sendDeviceAnnounce(uint16_t dstAddr) {
    // cluster=0x0013
    // [nwkAddr(2), extAddr(8), capability(1)]
    std::vector<uint8_t> payload;
    payload.push_back(m_nwkAddr & 0xFF);
    payload.push_back((m_nwkAddr >> 8) & 0xFF);
    for (int i = 0; i < 8; i++)
        payload.push_back((m_extAddr >> (8 * i)) & 0xFF);
    // capability – e.g. 0x8E if mains powered, rxOnIdle, security
    uint8_t capability = 0x8E;
    payload.push_back(capability);

    return sendZdoResponse(dstAddr, 0x0013, payload);
}

/********************************************************************
 * handleZdoFrame: dispatch
 ********************************************************************/
void ZigbeeZdo::handleZdoFrame(uint16_t srcAddr,
                               uint16_t clusterId,
                               const std::vector<uint8_t> &zdoPayload) {
    // Switch on clusterId
    switch (clusterId) {
        case 0x0000:
            handleNetworkAddrReq(srcAddr, zdoPayload);
            break;
        case 0x0001:
            handleIeeeAddrReq(srcAddr, zdoPayload);
            break;
        case 0x0002:
            handleNodeDescReq(srcAddr, zdoPayload);
            break;
        case 0x0003:
            handlePowerDescReq(srcAddr, zdoPayload);
            break;
        case 0x0004:
            handleSimpleDescReq(srcAddr, zdoPayload);
            break;
        case 0x0005:
            handleActiveEPReq(srcAddr, zdoPayload);
            break;
        case 0x0006:
            handleMatchDescReq(srcAddr, zdoPayload);
            break;

        case 0x0021:
            handleBindReq(srcAddr, zdoPayload);
            break;
        case 0x0022:
            handleUnbindReq(srcAddr, zdoPayload);
            break;

        case 0x0036:
            handleMgmtPermitJoinReq(srcAddr, zdoPayload);
            break;

        case 0x0013: // DeviceAnnce from remote
            // parse if needed
            // ...
            break;
        default:
            std::cout << "[ZDO] Unhandled clusterId=0x"
                      << std::hex << clusterId << "\n";
            break;
    }
}

/********************************************************************
 * NetworkAddrReq / Rsp
 ********************************************************************/
void ZigbeeZdo::handleNetworkAddrReq(uint16_t srcAddr,
                                     const std::vector<uint8_t> &payload) {
    // [IEEEAddr(8), reqType(1), startIndex(1)]
    if (payload.size() < 10) return;
    uint64_t reqExt = 0;
    for (int i = 0; i < 8; i++)
        reqExt |= ((uint64_t) payload[i]) << (8 * i);

    // If the requested IEEE matches ours
    if (reqExt == m_extAddr) {
        // build NetworkAddrRsp => cluster=0x8000
        // [status(1), IEEEAddr(8), nwkAddr(2), numAssocDev(1), startIndex(1), assocDevList...]
        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // status=SUCCESS
        for (int i = 0; i < 8; i++)
            rsp.push_back((m_extAddr >> (8 * i)) & 0xFF);
        rsp.push_back(m_nwkAddr & 0xFF);
        rsp.push_back((m_nwkAddr >> 8) & 0xFF);

        // no assoc dev
        rsp.push_back(0x00); // numAssoc
        rsp.push_back(payload[9]); // startIndex
        // no dev list

        sendZdoResponse(srcAddr, 0x8000, rsp);
    } else {
        // we do not know => might respond with NOT_FOUND or forward
        std::vector<uint8_t> rsp(1, 0x01); // status=FAIL
        sendZdoResponse(srcAddr, 0x8000, rsp);
    }
}

/********************************************************************
 * IeeeAddrReq / Rsp
 ********************************************************************/
void ZigbeeZdo::handleIeeeAddrReq(uint16_t srcAddr,
                                  const std::vector<uint8_t> &payload) {
    // [nwkAddr(2), reqType(1), startIndex(1)]
    if (payload.size() < 4) return;
    uint16_t reqNwk = payload[0] | (payload[1] << 8);

    if (reqNwk == m_nwkAddr) {
        // build IeeeAddrRsp => cluster=0x8001
        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // SUCCESS
        for (int i = 0; i < 8; i++)
            rsp.push_back((m_extAddr >> (8 * i)) & 0xFF);
        rsp.push_back(m_nwkAddr & 0xFF);
        rsp.push_back((m_nwkAddr >> 8) & 0xFF);
        rsp.push_back(0x00); // numAssocDev
        rsp.push_back(payload[2]); // startIndex
        // no list
        sendZdoResponse(srcAddr, 0x8001, rsp);
    } else {
        // not found
        std::vector<uint8_t> rsp(1, 0x01);
        sendZdoResponse(srcAddr, 0x8001, rsp);
    }
}

/********************************************************************
 * NodeDescReq / Rsp
 ********************************************************************/
void ZigbeeZdo::handleNodeDescReq(uint16_t srcAddr,
                                  const std::vector<uint8_t> &payload) {
    // [nwkAddr(2)]
    if (payload.size() < 2) return;
    uint16_t target = payload[0] | (payload[1] << 8);
    if (target == m_nwkAddr) {
        // build NodeDescRsp => cluster=0x8002
        // Format: [status(1), nwkAddr(2), nodeDesc(13 bytes?), see spec]
        // We'll do partial
        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // success
        rsp.push_back(m_nwkAddr & 0xFF);
        rsp.push_back((m_nwkAddr >> 8) & 0xFF);

        // nodeDesc. We'll fill some bits
        // In real code, you pack bitfields
        // We'll just put dummy
        rsp.push_back(m_nodeDesc.type);
        rsp.push_back((uint8_t) m_nodeDesc.manufacturerCode);
        // etc. for rest of 13 bytes

        sendZdoResponse(srcAddr, 0x8002, rsp);
    } else {
        // not for us => ignore or forward
    }
}

/********************************************************************
 * PowerDescReq / Rsp
 ********************************************************************/
void ZigbeeZdo::handlePowerDescReq(uint16_t srcAddr,
                                   const std::vector<uint8_t> &payload) {
    if (payload.size() < 2) return;
    uint16_t target = payload[0] | (payload[1] << 8);
    if (target == m_nwkAddr) {
        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // success
        rsp.push_back(m_nwkAddr & 0xFF);
        rsp.push_back((m_nwkAddr >> 8) & 0xFF);

        // pack power desc
        rsp.push_back(m_powerDesc.currentPowerMode);
        rsp.push_back(m_powerDesc.availablePowerSources);
        rsp.push_back(m_powerDesc.currentPowerSource);
        rsp.push_back(m_powerDesc.currentPowerSourceLevel);

        sendZdoResponse(srcAddr, 0x8003, rsp);
    }
}

/********************************************************************
 * SimpleDescReq / Rsp
 ********************************************************************/
void ZigbeeZdo::handleSimpleDescReq(uint16_t srcAddr,
                                    const std::vector<uint8_t> &payload) {
    if (payload.size() < 3) return;
    uint16_t target = payload[0] | (payload[1] << 8);
    uint8_t ep = payload[2];

    if (target == m_nwkAddr) {
        SimpleDescriptor desc;
        if (!m_endpointTable.getSimpleDescriptor(ep, desc)) {
            // error
            std::vector<uint8_t> rsp = {0x82, (uint8_t) (m_nwkAddr & 0xFF), (uint8_t) ((m_nwkAddr >> 8) & 0xFF)};
            sendZdoResponse(srcAddr, 0x8004, rsp);
            return;
        }

        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // success
        rsp.push_back(m_nwkAddr & 0xFF);
        rsp.push_back((m_nwkAddr >> 8) & 0xFF);

        // length placeholder
        size_t lenPos = rsp.size();
        rsp.push_back(0);

        rsp.push_back(desc.endpoint);
        rsp.push_back(desc.profileId & 0xFF);
        rsp.push_back((desc.profileId >> 8) & 0xFF);

        // In cluster
        rsp.push_back((uint8_t) desc.inClusterList.size());
        for (auto c: desc.inClusterList) {
            rsp.push_back(c & 0xFF);
            rsp.push_back((c >> 8) & 0xFF);
        }
        // Out cluster
        rsp.push_back((uint8_t) desc.outClusterList.size());
        for (auto c: desc.outClusterList) {
            rsp.push_back(c & 0xFF);
            rsp.push_back((c >> 8) & 0xFF);
        }

        rsp[lenPos] = (uint8_t) (rsp.size() - (lenPos + 1));
        sendZdoResponse(srcAddr, 0x8004, rsp);
    }
}

/********************************************************************
 * ActiveEPReq / Rsp
 ********************************************************************/
void ZigbeeZdo::handleActiveEPReq(uint16_t srcAddr,
                                  const std::vector<uint8_t> &payload) {
    if (payload.size() < 2) return;
    uint16_t target = payload[0] | (payload[1] << 8);
    if (target == m_nwkAddr) {
        auto eps = m_endpointTable.getActiveEndpoints();

        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // success
        rsp.push_back((uint8_t) (m_nwkAddr & 0xFF));
        rsp.push_back((uint8_t) ((m_nwkAddr >> 8) & 0xFF));
        rsp.push_back((uint8_t) eps.size());
        for (auto e: eps)
            rsp.push_back(e);

        sendZdoResponse(srcAddr, 0x8005, rsp);
    }
}

/********************************************************************
 * MatchDescReq / Rsp
 ********************************************************************/
void ZigbeeZdo::handleMatchDescReq(uint16_t srcAddr,
                                   const std::vector<uint8_t> &payload) {
    // [nwkAddr(2), profileId(2), inCount(1), inClusters..., outCount(1), outClusters...]
    // We'll do a minimal approach
    if (payload.size() < 5) return;
    uint16_t target = payload[0] | (payload[1] << 8);
    uint16_t profile = payload[2] | (payload[3] << 8);
    uint8_t inCount = payload[4];
    size_t offset = 5;
    if (payload.size() < offset + inCount * 2) return;

    std::vector<uint16_t> inCl;
    for (int i = 0; i < inCount; i++) {
        uint16_t cid = payload[offset] | (payload[offset + 1] << 8);
        offset += 2;
        inCl.push_back(cid);
    }
    if (payload.size() < offset + 1) return;
    uint8_t outCount = payload[offset++];
    if (payload.size() < offset + outCount * 2) return;
    std::vector<uint16_t> outCl;
    for (int i = 0; i < outCount; i++) {
        uint16_t cid = payload[offset] | (payload[offset + 1] << 8);
        offset += 2;
        outCl.push_back(cid);
    }

    if (target == m_nwkAddr) {
        // find endpoints that match
        auto eps = m_endpointTable.getActiveEndpoints();
        std::vector<uint8_t> matched;

        for (auto ep: eps) {
            SimpleDescriptor desc;
            if (m_endpointTable.getSimpleDescriptor(ep, desc)) {
                if (desc.profileId == profile) {
                    // check if intersects inClusterList with inCl, outClusterList with outCl
                    bool inOk = false, outOk = false;
                    if (inCl.empty()) inOk = true;
                    else {
                        for (auto c: desc.inClusterList) {
                            if (std::find(inCl.begin(), inCl.end(), c) != inCl.end()) {
                                inOk = true;
                                break;
                            }
                        }
                    }
                    if (outCl.empty()) outOk = true;
                    else {
                        for (auto c: desc.outClusterList) {
                            if (std::find(outCl.begin(), outCl.end(), c) != outCl.end()) {
                                outOk = true;
                                break;
                            }
                        }
                    }
                    if (inOk && outOk) {
                        matched.push_back(ep);
                    }
                }
            }
        }
        // build matchdesc rsp => 0x8006
        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // success
        rsp.push_back((uint8_t) (m_nwkAddr & 0xFF));
        rsp.push_back((uint8_t) ((m_nwkAddr >> 8) & 0xFF));
        rsp.push_back((uint8_t) matched.size());
        for (auto e: matched)
            rsp.push_back(e);

        sendZdoResponse(srcAddr, 0x8006, rsp);
    }
}

/********************************************************************
 * Bind / Unbind Req / Rsp
 ********************************************************************/
void ZigbeeZdo::handleBindReq(uint16_t srcAddr,
                              const std::vector<uint8_t> &payload) {
    // [srcIEEE(8), srcEndpoint(1), clusterId(2), dstAddrMode(1), ...]
    // Typically you'd store in a binding table
    // We'll just respond success
    std::vector<uint8_t> rsp;
    rsp.push_back(0x00); // success
    sendZdoResponse(srcAddr, 0x8021, rsp);
}

void ZigbeeZdo::handleUnbindReq(uint16_t srcAddr,
                                const std::vector<uint8_t> &payload) {
    // remove from binding table
    std::vector<uint8_t> rsp;
    rsp.push_back(0x00); // success
    sendZdoResponse(srcAddr, 0x8022, rsp);
}

/********************************************************************
 * MgmtPermitJoinReq / Rsp
 ********************************************************************/
void ZigbeeZdo::handleMgmtPermitJoinReq(uint16_t srcAddr,
                                        const std::vector<uint8_t> &payload) {
    // [permitDuration(1), tcSignificance(1)]
    if (payload.size() < 2) return;
    uint8_t duration = payload[0];
    uint8_t tcSig = payload[1];

    std::cout << "[ZDO] MgmtPermitJoinReq => set permit join for " << (int) duration << " sec\n";
    // you'd call NWK or MAC to allow join
    // respond
    std::vector<uint8_t> rsp;
    rsp.push_back(0x00); // success
    sendZdoResponse(srcAddr, 0x8036, rsp);
}

/********************************************************************
 * Utility to sendZdoResponse
 ********************************************************************/
bool ZigbeeZdo::sendZdoResponse(uint16_t dstAddr,
                                uint16_t clusterId,
                                const std::vector<uint8_t> &payload) {
    // Typically, we do an APS unicast on endpoint=0, same clusterId
    return m_aps.sendApsData(dstAddr, clusterId, payload, /*srcEp=*/0, /*dstEp=*/0, false);
}

void ZigbeeZdo::handleMgmtLeaveReq(uint16_t srcAddr,
                                   const std::vector<uint8_t> &payload) {
    // [deviceExtAddr(8), removeChildrenRejoin(1)]
    if (payload.size() < 9) return;
    uint64_t devExt = 0;
    for (int i = 0; i < 8; i++)
        devExt |= ((uint64_t) payload[i]) << (8 * i);
    uint8_t flags = payload[8];
    bool removeChildren = (flags & 0x02) != 0;
    bool rejoin = (flags & 0x01) != 0;

    std::cout << "[ZDO] MgmtLeaveReq from 0x" << std::hex << srcAddr
              << ": ext=0x" << devExt
              << " removeChildren=" << removeChildren
              << " rejoin=" << rejoin << "\n";

    // If devExt==our extAddr => we leave the network. Real code calls NWK, resets state, etc.
    // Possibly we do NWK remove child if removeChildren==true.

    // build MgmtLeaveRsp => 0x8034
    std::vector<uint8_t> rsp;
    rsp.push_back(0x00); // success
    sendZdoResponse(srcAddr, 0x8034, rsp);
}

void ZigbeeZdo::handleMgmtLqiReq(uint16_t srcAddr,
                                 const std::vector<uint8_t> &payload) {
    // [startIndex(1)]
    if (payload.size() < 1) return;
    uint8_t startIndex = payload[0];

    // We gather neighbor table from NWK or from an internal structure. Suppose we have 3 neighbors
    // In real code, you'd retrieve from NWK layer's neighbor table.
    // For demonstration, we'll pretend we have none or a stub.

    uint8_t neighborCount = 0;
    uint8_t neighborTableSize = 0;
    uint8_t tableEntriesCount = 0; // how many we will list in this response

    // Build MgmtLqiRsp => cluster=0x8031
    // [status(1), neighborTableEntries(1), startIndex(1), neighborCount(1), {neighbor entries...}]

    std::vector<uint8_t> rsp;
    rsp.push_back(0x00); // success
    rsp.push_back(neighborTableSize);
    rsp.push_back(startIndex);
    rsp.push_back(tableEntriesCount);

    // No neighbor entries for now (or fill them if you have them)

    sendZdoResponse(srcAddr, 0x8031, rsp);
}

void ZigbeeZdo::handleMgmtNwkUpdateReq(uint16_t srcAddr,
                                       const std::vector<uint8_t> &payload) {
    // [scanChannels(4B), scanDuration(1B), scanCount(1B, optional), nwkUpdateId(1B, optional), etc...]
    if (payload.size() < 5) return;
    uint32_t scanChannels = payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24);
    uint8_t scanDuration = payload[4];

    std::cout << "[ZDO] MgmtNwkUpdateReq: channels=0x"
              << std::hex << scanChannels
              << " duration=" << std::dec << (int) scanDuration << "\n";

    // If scanDuration<0x05 => energy scan.
    // If 0xFE => change channel. If 0xFF => other special meaning.
    // Real code calls NWK for channel changes or scanning.

    // Typically you eventually send a MgmtNwkUpdateNotify => 0x8038
    std::vector<uint8_t> notify;
    notify.push_back(0x00); // status=SUCCESS
    notify.push_back((uint8_t) scanDuration);
    // possibly a list of energy scan results
    sendZdoResponse(srcAddr, 0x8038, notify);
}

void ZigbeeZdo::handleUserDescReq(uint16_t srcAddr,
                                  const std::vector<uint8_t> &payload) {
    // [nwkAddr(2)]
    if (payload.size() < 2) return;
    uint16_t target = payload[0] | (payload[1] << 8);
    if (target == m_nwkAddr) {
        // build UserDescRsp => 0x8014
        // [status(1), nwkAddr(2), length(1), userDescVar...]
        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // success
        rsp.push_back(m_nwkAddr & 0xFF);
        rsp.push_back((m_nwkAddr >> 8) & 0xFF);

        uint8_t descLen = (uint8_t) std::min<size_t>(m_userDesc.userDesc.size(), 16);
        rsp.push_back(descLen);
        for (int i = 0; i < descLen; i++)
            rsp.push_back(m_userDesc.userDesc[i]);

        sendZdoResponse(srcAddr, 0x8014, rsp);
    }
}

void ZigbeeZdo::handleUserDescSet(uint16_t srcAddr,
                                  const std::vector<uint8_t> &payload) {
    // [nwkAddr(2), length(1), userDescVar...]
    if (payload.size() < 3) return;
    uint16_t target = payload[0] | (payload[1] << 8);
    uint8_t length = payload[2];
    if (payload.size() < 3 + length) return;

    if (target == m_nwkAddr) {
        // update
        m_userDesc.userDesc = std::string((const char *) (&payload[3]), (const char *) (&payload[3] + length));
        std::cout << "[ZDO] Updated local user descriptor=" << m_userDesc.userDesc << "\n";
    }
    // send UserDescConf => 0x8015
    std::vector<uint8_t> rsp;
    rsp.push_back(0x00); // success
    sendZdoResponse(srcAddr, 0x8015, rsp);
}

void ZigbeeZdo::handleComplexDescReq(uint16_t srcAddr,
                                     const std::vector<uint8_t> &payload) {
    // [nwkAddr(2)]
    if (payload.size() < 2) return;
    uint16_t target = payload[0] | (payload[1] << 8);
    if (target == m_nwkAddr) {
        // build ComplexDescRsp => 0x8010
        // Real structure is more complicated
        std::vector<uint8_t> rsp;
        rsp.push_back(0x00); // success
        rsp.push_back(m_nwkAddr & 0xFF);
        rsp.push_back((m_nwkAddr >> 8) & 0xFF);

        // length placeholder
        size_t lenPos = rsp.size();
        rsp.push_back(0);

        // store info from m_complexDesc
        std::string &info = m_complexDesc.info;
        uint8_t infoLen = (uint8_t) std::min<size_t>(info.size(), 32);
        for (int i = 0; i < infoLen; i++)
            rsp.push_back(info[i]);

        rsp[lenPos] = (uint8_t) (rsp.size() - (lenPos + 1));

        sendZdoResponse(srcAddr, 0x8010, rsp);
    }
}

void ZigbeeZdo::start() {
    m_aps.setApsIndicationCallback(
            [this](uint8_t dstEp, uint16_t clusterId, const std::vector<uint8_t> &payload,
                   uint16_t srcAddr, uint8_t srcEp) {
                if (dstEp == 0 && clusterId < 0x0040)
                    this->handleZdoFrame(srcAddr, clusterId, payload);
                else; // pass to ZCL or ignore
            }
    );
    m_aps.start();
}