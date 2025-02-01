#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include "ZigbeeMac.h"
#include "SecurityManager.h"

using NwkFrameIndicationCallback = std::function<void(const std::vector<uint8_t> &nwkPayload,
        uint16_t srcAddr,
uint16_t dstAddr)>;

class ZigbeeNwk
{
public:
    ZigbeeNwk(ZigbeeMac &mac, SecurityManager &sec);
    ~ZigbeeNwk();

    void start();
    void stop();

    void setFrameIndicationCallback(NwkFrameIndicationCallback cb);

    // Outbound NWK data
    bool sendNwkFrame(const std::vector<uint8_t> &payload, uint16_t dstAddr);

private:
    // Inbound from MAC
    void handleMacFrame(const std::vector<uint8_t> &macFrame);

    // TX done from MAC
    void handleMacTxDone(bool success);

    // NWK encrypt / decrypt
    bool nwkEncrypt(std::vector<uint8_t> &macFrame);
    bool nwkDecrypt(std::vector<uint8_t> &macFrame);

    ZigbeeMac &m_mac;
    SecurityManager &m_sec;

    std::mutex m_mutex;
    bool m_running;
    NwkFrameIndicationCallback m_cb;
};
