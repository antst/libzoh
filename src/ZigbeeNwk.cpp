#include "../include/ZigbeeNwk.h"
#include <iostream>

ZigbeeNwk::ZigbeeNwk(ZigbeeMac &mac, SecurityManager &sec)
        : m_mac(mac)
        , m_sec(sec)
        , m_running(false)
{
}

ZigbeeNwk::~ZigbeeNwk()
{
    stop();
}

void ZigbeeNwk::start()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_running)
    {
        m_running=true;
        // set mac callbacks
        m_mac.setMacFrameHandler(
                [this](const std::vector<uint8_t> &frame){ handleMacFrame(frame); }
        );
        m_mac.setMacTxDoneHandler(
                [this](bool success){ handleMacTxDone(success); }
        );
        m_mac.start();
    }
}

void ZigbeeNwk::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_running)
    {
        m_running=false;
        m_mac.setMacFrameHandler(nullptr);
        m_mac.setMacTxDoneHandler(nullptr);
        m_mac.stop();
    }
}

void ZigbeeNwk::setFrameIndicationCallback(NwkFrameIndicationCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cb= cb;
}

bool ZigbeeNwk::sendNwkFrame(const std::vector<uint8_t> &payload, uint16_t dstAddr)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_running) return false;
    // Build NWK header + do NWK encryption
    // e.g. [FC, seq, dst, src, radius, security aux?] + payload
    std::vector<uint8_t> macFrame;
    // For demonstration
    macFrame.push_back(0x29); // NWK FC stub
    macFrame.push_back(0xAB); // seq stub
    macFrame.push_back(dstAddr &0xFF);
    macFrame.push_back((dstAddr>>8)&0xFF);
    // ...
    macFrame.insert(macFrame.end(), payload.begin(), payload.end());

    // NWK encrypt
    if(!nwkEncrypt(macFrame))
        return false;

    return m_mac.sendFrame(macFrame);
}

// inbound from MAC
void ZigbeeNwk::handleMacFrame(const std::vector<uint8_t> &macFrame)
{
    // parse NWK header
    // do NWK decrypt
    std::vector<uint8_t> frameCopy= macFrame;
    if(!nwkDecrypt(frameCopy))
        return;

    // minimal parse
    if(frameCopy.size()<4) return;
    uint16_t dst= frameCopy[2]|(frameCopy[3]<<8);
    uint16_t src= 0x0001; // stub
    // remove NWK header
    if(frameCopy.size()>8)
    {
        std::vector<uint8_t> nwkPayload(frameCopy.begin()+8, frameCopy.end());
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_cb)
        {
            m_cb(nwkPayload, src, dst);
        }
    }
}

void ZigbeeNwk::handleMacTxDone(bool success)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!success)
    {
        std::cout<<"[NWK] MAC TX fail => route repair?\n";
        // route repair logic if needed
    }
}

bool ZigbeeNwk::nwkEncrypt(std::vector<uint8_t> &macFrame)
{
    // call security manager
    uint32_t frameCounter=0;
    std::vector<uint8_t> cipher;
    bool ok= m_sec.nwkEncrypt(macFrame, frameCounter, cipher);
    if(!ok) return false;
    macFrame= cipher;
    return true;
}

bool ZigbeeNwk::nwkDecrypt(std::vector<uint8_t> &macFrame)
{
    return m_sec.nwkDecrypt(macFrame);
}
