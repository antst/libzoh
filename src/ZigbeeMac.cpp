#include "ZigbeeMac.h"
#include <iostream>
#include <algorithm>

// Example spinel property for "TX status" events
static constexpr uint16_t SPINEL_PROP_ZB_TX_STATUS = 0x1010;

// Example spinel property for "Association Indication" events
static constexpr uint16_t SPINEL_PROP_ZB_ASSOC_IND = 0x1011;

// Example spinel property for "Association Confirm" if device side
static constexpr uint16_t SPINEL_PROP_ZB_ASSOC_CNF = 0x1012;

ZigbeeMac::ZigbeeMac(RawSpinelDriver &spinelDriver)
        : m_spinel(spinelDriver), m_running(false), m_macAssocEnabled(false) {
}

ZigbeeMac::~ZigbeeMac() {
    stop();
}

void ZigbeeMac::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) {
        m_running = true;
        // Set inbound raw frame callback
        m_spinel.setRawFrameHandler(
                [this](const std::vector<uint8_t> &frame, const int8_t rssi, const uint8_t lqi) {
                    handleInboundRaw(frame,rssi,lqi);
                }
        );
        // Set spinel event callback for ack or assoc
        m_spinel.setSpinelEventHandler(
                [this](uint16_t propId, const std::vector<uint8_t> &data) {
                    handleSpinelEvent(propId, data);
                }
        );
        m_spinel.start();
    }
}

void ZigbeeMac::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) {
        m_running = false;
        m_spinel.setRawFrameHandler(nullptr);
        m_spinel.setSpinelEventHandler(nullptr);
        m_spinel.stop();
    }
}

void ZigbeeMac::setMacFrameHandler(MacFrameHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameHandler = handler;
}

void ZigbeeMac::setMacTxDoneHandler(MacTxDoneHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_txDoneHandler = handler;
}

bool ZigbeeMac::setChannel(uint8_t channel) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_spinel.setChannel(channel);
}

bool ZigbeeMac::setPanId(uint16_t panId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_spinel.setPanId(panId);
}

bool ZigbeeMac::setExtendedAddress(uint64_t extAddr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_spinel.setExtendedAddress(extAddr);
}

bool ZigbeeMac::sendFrame(const std::vector<uint8_t> &frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // NWK or higher has built the full MAC header
    // Just pass to spinel
    return m_spinel.sendRawFrame(frame);
}

void ZigbeeMac::enableMacAssociation(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_macAssocEnabled = enable;
    // Possibly set a spinel property if RCP handles assoc?
}

bool ZigbeeMac::isMacAssociationEnabled() {
    // We only need a lock if we do any modification, but to be safe:
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_macAssocEnabled;
}

/********************************************************************
 * handleInboundRaw
 ********************************************************************/
void ZigbeeMac::handleInboundRaw(const std::vector<uint8_t> &frame, const int8_t rssi, const uint8_t lqi) {
    // If MAC association is enabled, check if this is an association request or response
    if (m_macAssocEnabled && frame.size() > 3) {
        // Example: check MAC FrameControl for 0xC0=Assoc request?
        uint16_t fc = (frame[0] | (frame[1] << 8));
        // This is purely illustrative. Real 802.15.4 parsing is more involved.
        bool isAssocReq = ((fc & 0xFFC0) == 0xC000);

        if (isAssocReq) {
            handleAssociationRequest(frame);
            return;
        }
        bool isAssocResp = ((fc & 0xFFC0) == 0xC080);
        if (isAssocResp) {
            handleAssociationResponse(frame);
            return;
        }
    }

    // Otherwise pass up to NWK
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frameHandler) {
        m_frameHandler(frame,rssi,lqi);
    }
}

/********************************************************************
 * handleSpinelEvent
 *  - e.g. if spinel firmware sends "TX status" or "Association Ind/Conf"
 ********************************************************************/
void ZigbeeMac::handleSpinelEvent(uint16_t propId, const std::vector<uint8_t> &data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (propId == SPINEL_PROP_ZB_TX_STATUS) {
// data might be [status(1), maybe seq or TID]
        if (data.size() < 1) return;
        uint8_t status = data[0];
        bool success = (status == 0); // 0= success, nonzero= fail
        if (m_txDoneHandler) {
            m_txDoneHandler(success);
        }
    } else if (propId == SPINEL_PROP_ZB_ASSOC_IND && m_macAssocEnabled) {
// e.g. data= [extAddr(8), capability(1)]
// If coordinator, accept
// build an association response, or call NWK
// We'll do a simple stub
        std::cout << "[MAC] AssocInd from spinel\n";
    } else if (propId == SPINEL_PROP_ZB_ASSOC_CNF && m_macAssocEnabled) {
// e.g. data= [status(1), shortAddr(2)]
// If device side, confirm association
        if (data.size() < 3) return;
        uint8_t status = data[0];
        uint16_t shortAddr = data[1] | (data[2] << 8);
        std::cout << "[MAC] AssocCnf => status=" << (int) status << " short=0x" << std::hex << shortAddr << "\n";
    } else {
// Some other property event => ignore or log
        std::cout << "[MAC] spinel event propId=0x" << std::hex << propId << " dataLen=" << std::dec << data.size()
                  << "\n";
    }
}

/********************************************************************
 * handleAssociationRequest
 *   - If coordinator, respond with an association response
 ********************************************************************/
void ZigbeeMac::handleAssociationRequest(const std::vector<uint8_t> &frame) {
    // parse source addr from MAC header, parse capability
    // In real code, we do complete 802.15.4 parse. This is stubbed.
    uint16_t srcShort = 0x9999; // stub
    uint8_t capability = 0x8E;

    // Decide short address for the device, e.g. next free
    uint16_t allocatedAddr = 0x0040;

    // build association response frame
    // in real code, we build 802.15.4 MAC assoc response
    std::vector<uint8_t> respFrame = {0xC0, 0x80, /*fc for assoc resp*/ 0x02/*seq?*/ };
    // fill in allocatedAddr
    respFrame.push_back(allocatedAddr & 0xFF);
    respFrame.push_back((allocatedAddr >> 8) & 0xFF);
    // success=0

    sendFrame(respFrame);
    std::cout << "[MAC] Sent association response => short=0x" << std::hex << allocatedAddr << "\n";
}

/********************************************************************
 * handleAssociationResponse
 *   - If we are the device side, read the short address
 ********************************************************************/
void ZigbeeMac::handleAssociationResponse(const std::vector<uint8_t> &frame) {
    // parse short address. Stub
    uint16_t newAddr = 0x0040;
    std::cout << "[MAC] Device got assoc response => short=0x" << std::hex << newAddr << "\n";
}
