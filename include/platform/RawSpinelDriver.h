#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "SerialPort.h" // or your own serial interface

/**
 * Callback when a raw 802.15.4 frame arrives from the RCP.
 */
using RawFrameHandler = std::function<void(const std::vector<uint8_t> &frame)>;

/**
 * Callback for property changes or spinel events (like channel set confirm).
 *   propertyId: which spinel property
 *   data: payload of the property
 */
using SpinelEventHandler = std::function<void(uint16_t propertyId,
const std::vector<uint8_t> &data)>;

/**
 * Production-ready raw Spinel driver that uses HDLC framing + CRC over UART.
 * - It can set / get spinel properties (channel, panid, etc.).
 * - It can send raw 802.15.4 frames with hardware-level MAC ack in the RCP.
 * - It spawns a read thread to parse inbound frames and dispatch them.
 */
class RawSpinelDriver
{
public:
    RawSpinelDriver(const std::string &portName, int baudRate);
    ~RawSpinelDriver();

    // Start the background read thread
    void start();
    // Stop the read thread and close port
    void stop();

    // Set callback for inbound raw frames
    void setRawFrameHandler(RawFrameHandler handler);

    // Optional callback for spinel property events
    void setSpinelEventHandler(SpinelEventHandler handler);

    // Send a raw 802.15.4 frame to the RCP
    bool sendRawFrame(const std::vector<uint8_t> &frame);

    // Example: set channel property
    bool setChannel(uint8_t channel);

    // Example: set PanId property
    bool setPanId(uint16_t panId);

    // Example: set extended address property
    bool setExtendedAddress(uint64_t extAddr);

private:
    // The main read loop that reads from UART, decodes HDLC, dispatches Spinel frames
    void rxLoop();

    // HDLC encode + send
    bool sendSpinelFrame(const std::vector<uint8_t> &spinelPayload);

    // Parse a spinel frame (already HDLC-decoded)
    void handleSpinelFrame(const std::vector<uint8_t> &spinelFrame);

    // Build / parse spinel commands
    bool sendSpinelSetProp(uint16_t propId, const std::vector<uint8_t> &data);

    // Possibly you implement getProp, handle spinel acks, etc.

    // HDLC helpers
    std::vector<uint8_t> hdlcEncode(const uint8_t* data, size_t len);
    bool hdlcDecode(const std::vector<uint8_t> &input,
                    std::vector<uint8_t> &output);

private:
    SerialPort m_port;
    std::thread m_rxThread;
    std::atomic<bool> m_running;

    RawFrameHandler    m_rawFrameHandler;
    SpinelEventHandler m_spinelEventHandler;

    // For concurrency in sending frames
    std::mutex m_sendMutex;
};
