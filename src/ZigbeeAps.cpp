#include "../include/ZigbeeAps.h"
#include <iostream>

ZigbeeAps::ZigbeeAps(ZigbeeNwk &nwk, SecurityManager &sec)
        : m_nwk(nwk), m_sec(sec), m_running(false) {
}

ZigbeeAps::~ZigbeeAps() {
    stop();
}

void ZigbeeAps::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) {
        m_running = true;
        m_nwk.setNwkPayloadCallback(
                [this](const std::vector<uint8_t> &nwkPayload, uint16_t srcAddr, uint16_t dstAddr) {
                    this->handleNwkPayload(nwkPayload, srcAddr, dstAddr);
                }
        );
        m_nwk.start();
    }
}

void ZigbeeAps::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) {
        m_running = false;
        m_nwk.setNwkPayloadCallback(nullptr);
        m_nwk.stop();
    }
}

void ZigbeeAps::setApsDataIndicationCallback(ApsDataIndicationCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_apsCb = cb;
}

void ZigbeeAps::setApsIndicationCallback(ApsIndicationCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cb = cb;
}

bool ZigbeeAps::sendApsData(uint16_t dstAddr, uint16_t clusterId,
                            const std::vector<uint8_t> &payload,
                            uint8_t srcEp, uint8_t dstEp, bool apsSecurity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) return false;

// Build APS header
// e.g. [frameCtrl(1B), apsCounter(1B), dstEp(1B), clusterId(2B), srcEp(1B), payload...]
    std::vector<uint8_t> apsFrame;
    uint8_t frameCtrl = (apsSecurity ? 0x40 : 0x00); // bit6 => security
    apsFrame.push_back(frameCtrl);
    apsFrame.push_back(0x22); // APS counter
    apsFrame.push_back(dstEp);
    apsFrame.push_back(clusterId & 0xFF);
    apsFrame.push_back((clusterId >> 8) & 0xFF);
    apsFrame.push_back(srcEp);

    apsFrame.insert(apsFrame.end(), payload.begin(), payload.end());

// If APS security, do m_sec.apsEncrypt
    if (apsSecurity) {
        uint64_t linkExt = 0x1122334455667788ULL; // stub (destination ext) or trust center
        std::vector<uint8_t> cipher;
        if (!m_sec.apsEncrypt(apsFrame, linkExt, cipher))
            return false;
        apsFrame = cipher;
    }

// pass to NWK
    return m_nwk.sendNwkFrame(apsFrame, dstAddr);
}

// inbound from NWK
void ZigbeeAps::handleNwkPayload(const std::vector<uint8_t> &nwkPayload,
                                 uint16_t srcAddr, uint16_t dstAddr) {
    // parse APS header
    if (nwkPayload.size() < 6) return;
    uint8_t frameCtrl = nwkPayload[0];
    bool sec = ((frameCtrl & 0x40) != 0);
    uint8_t apsCounter = nwkPayload[1];
    uint8_t dstEp = nwkPayload[2];
    uint16_t cluster = nwkPayload[3] | (nwkPayload[4] << 8);
    uint8_t srcEp = nwkPayload[5];

    // if APS security, do m_sec.apsDecrypt
    std::vector<uint8_t> appPayload(nwkPayload.begin() + 6, nwkPayload.end());
    if (sec) {
        uint64_t linkExt = 0x1122334455667788ULL;
        std::vector<uint8_t> frameCopy(nwkPayload);
        if (!m_sec.apsDecrypt(frameCopy, linkExt)) {
            std::cerr << "[APS] decrypt fail\n";
            return;
        }
        // update appPayload from frameCopy if needed
    }

    // dispatch
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cb) {
        m_cb(dstEp, cluster, appPayload, srcAddr, srcEp);
    }
}
