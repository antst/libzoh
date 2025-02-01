#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include "platform/RawSpinelDriver.h"

/**
 * Callback when a raw MAC frame is received from the radio.
 *  - frame: entire 802.15.4 MAC frame
 */
using MacFrameHandler = std::function<void(const std::vector<uint8_t> &frame,
                                           int8_t rssi,
                                           uint8_t lqi)>;

/**
 * Callback for TX done events:
 *  - success: true if ack received, false if ack failure/no ack
 */
using MacTxDoneHandler = std::function<void(bool success)>;

/**
 * ZigbeeMac:
 *   - Bridges host stack to RCP via RawSpinelDriver in "raw" 802.15.4 mode
 *   - Properly handles ack failures if RCP signals them
 *   - Allows (optional) MAC association (coordinator side or device side)
 *   - Ensures thread safety with mutexes
 */
class ZigbeeMac {
public:
    ZigbeeMac(RawSpinelDriver &spinelDriver);

    ~ZigbeeMac();

    // Start the MAC driver: sets inbound callbacks, starts RCP if not started
    void start();

    // Stop: clear callbacks, stop RCP
    void stop();

    // Set callback for inbound 802.15.4 frames
    void setMacFrameHandler(MacFrameHandler handler);

    // Set callback for TX done events
    void setMacTxDoneHandler(MacTxDoneHandler handler);

    // Basic config
    bool setChannel(uint8_t channel);

    bool setPanId(uint16_t panId);

    bool setExtendedAddress(uint64_t extAddr);

    // Send a raw 802.15.4 frame. NWK or higher builds the MAC header
    bool sendFrame(const std::vector<uint8_t> &frame);

    // Optional MAC association if your device wants it at MAC layer
    // If coordinator, accept association requests
    // If device, send association request
    // (In many Zigbee 3.0 scenarios, NWK-level association is used instead)
    void enableMacAssociation(bool enable);

    bool isMacAssociationEnabled();

private:
    // Inbound raw frame from RCP
    void handleInboundRaw(const std::vector<uint8_t> &frame);

    // Spinel event handler for ack success/fail, association confirmations, etc.
    void handleSpinelEvent(uint16_t propId, const std::vector<uint8_t> &data);

    // Parse association requests/indications if MAC assoc is enabled
    void handleAssociationRequest(const std::vector<uint8_t> &frame);

    void handleAssociationResponse(const std::vector<uint8_t> &frame);

private:
    RawSpinelDriver &m_spinel;
    bool m_running;

    // Thread safety
    std::mutex m_mutex;

    MacFrameHandler m_frameHandler;
    MacTxDoneHandler m_txDoneHandler;

    bool m_macAssocEnabled;
};
