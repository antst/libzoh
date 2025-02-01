#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <functional>
#include "ZigbeeNwk.h"
#include "SecurityManager.h"

using ApsIndicationCallback = std::function<void(uint8_t dstEndpoint,
                                                 uint16_t clusterId,
                                                 const std::vector<uint8_t> &payload,
                                                 uint16_t srcAddr,
                                                 uint8_t srcEp)>;

using ApsDataIndicationCallback = std::function<void(uint16_t srcAddr,
                                                     uint16_t clusterId,
                                                     const std::vector<uint8_t> &apsPayload)>;


class ZigbeeAps {
public:
    ZigbeeAps(ZigbeeNwk &nwk, SecurityManager &sec);

    ~ZigbeeAps();

    void start();

    void stop();

    void setApsIndicationCallback(ApsIndicationCallback cb);

    void setApsDataIndicationCallback(ApsDataIndicationCallback cb);

    bool sendApsData(uint16_t dstAddr, uint16_t clusterId,
                     const std::vector<uint8_t> &payload,
                     uint8_t srcEp, uint8_t dstEp,
                     bool apsSecurity);

private:
    void handleNwkPayload(const std::vector<uint8_t> &nwkPayload,
                          uint16_t srcAddr, uint16_t dstAddr);

    ZigbeeNwk &m_nwk;
    SecurityManager &m_sec;
    ApsIndicationCallback m_cb;
    ApsDataIndicationCallback m_apsCb;
    bool m_running;
    std::mutex m_mutex;
};
