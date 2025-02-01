#include "../../include/platform/RawSpinelDriver.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

// HDLC constants
static constexpr uint8_t HDLC_FLAG  = 0x7E;
static constexpr uint8_t HDLC_ESCAPE= 0x7D;
static constexpr uint8_t HDLC_XOR   = 0x20;

// Spinel basic definitions
static constexpr uint8_t SPINEL_HEADER_FLAG     = 0x80;
static constexpr uint16_t SPINEL_PROP_STREAM_RAW= 0x61;

// Example property IDs for channel, panid, extAddr in a “Zigbee spinel”
static constexpr uint16_t SPINEL_PROP_ZB_CHANNEL        = 0x1000;
static constexpr uint16_t SPINEL_PROP_ZB_PANID          = 0x1001;
static constexpr uint16_t SPINEL_PROP_ZB_EXTENDED_ADDR  = 0x1002;

// Spinel commands
enum SpinelCommand : uint16_t {
    SPINEL_CMD_NOOP            = 0,
    SPINEL_CMD_PROP_VALUE_GET  = 2,
    SPINEL_CMD_PROP_VALUE_SET  = 3,
    SPINEL_CMD_PROP_VALUE_IS   = 4,
};

/********************************************************************
 * Constructor / Destructor
 ********************************************************************/
RawSpinelDriver::RawSpinelDriver(const std::string &portName, int baudRate)
        : m_port(portName, baudRate)
        , m_running(false)
        , m_rawFrameHandler(nullptr)
        , m_spinelEventHandler(nullptr)
{
}

RawSpinelDriver::~RawSpinelDriver()
{
    stop();
}

/********************************************************************
 * start/stop
 ********************************************************************/
void RawSpinelDriver::start()
{
    if(!m_running)
    {
        m_running=true;
        m_rxThread= std::thread(&RawSpinelDriver::rxLoop, this);
    }
}

void RawSpinelDriver::stop()
{
    m_running=false;
    if(m_rxThread.joinable())
        m_rxThread.join();
}

/********************************************************************
 * setRawFrameHandler / setSpinelEventHandler
 ********************************************************************/
void RawSpinelDriver::setRawFrameHandler(RawFrameHandler handler)
{
    m_rawFrameHandler= handler;
}
void RawSpinelDriver::setSpinelEventHandler(SpinelEventHandler handler)
{
    m_spinelEventHandler= handler;
}

/********************************************************************
 * sendRawFrame
 *   - build spinel “PROP_VALUE_SET, property=SPINEL_PROP_STREAM_RAW”
 *   - payload is the 802.15.4 frame
 *   - HDLC-encode & write
 ********************************************************************/
bool RawSpinelDriver::sendRawFrame(const std::vector<uint8_t> &frame)
{
    // Build spinel: [header(1B), command(2B), propId(2B), payload...]
    // header= 0x80 (flag) + TID=1? (for example)
    uint8_t headerByte= SPINEL_HEADER_FLAG | (1 & 0x0F);

    std::vector<uint8_t> spinel;
    spinel.push_back(headerByte);

    // command= PropValueSet
    spinel.push_back((uint8_t)(SPINEL_CMD_PROP_VALUE_SET & 0xFF));
    spinel.push_back((uint8_t)((SPINEL_CMD_PROP_VALUE_SET>>8)&0xFF));

    // propId= STREAM_RAW (0x61)
    uint16_t prop= SPINEL_PROP_STREAM_RAW;
    spinel.push_back((uint8_t)(prop &0xFF));
    spinel.push_back((uint8_t)((prop>>8)&0xFF));

    // payload= the raw 802.15.4 frame
    spinel.insert(spinel.end(), frame.begin(), frame.end());

    return sendSpinelFrame(spinel);
}

/********************************************************************
 * setChannel, setPanId, setExtendedAddress
 *   - Example spinel “property set” requests
 ********************************************************************/
bool RawSpinelDriver::setChannel(uint8_t channel)
{
std::vector<uint8_t> data;
data.push_back(channel);
return sendSpinelSetProp(SPINEL_PROP_ZB_CHANNEL, data);
}

bool RawSpinelDriver::setPanId(uint16_t panId)
{
std::vector<uint8_t> data;
data.push_back((uint8_t)(panId &0xFF));
data.push_back((uint8_t)((panId>>8)&0xFF));
return sendSpinelSetProp(SPINEL_PROP_ZB_PANID, data);
}

bool RawSpinelDriver::setExtendedAddress(uint64_t extAddr)
{
    std::vector<uint8_t> data(8,0);
    for(int i=0;i<8;i++)
        data[i]= (uint8_t)((extAddr>>(8*i)) &0xFF);
    return sendSpinelSetProp(SPINEL_PROP_ZB_EXTENDED_ADDR, data);
}

/********************************************************************
 * sendSpinelSetProp
 *   - Build spinel frame: [header, cmd=PROP_VALUE_SET, propId, data...]
 *   - Synchronous ack or just best-effort?
 *   - For demonstration, we just send it.
 ********************************************************************/
bool RawSpinelDriver::sendSpinelSetProp(uint16_t propId,
const std::vector<uint8_t> &data)
{
// build [header(1B), cmd(2B), propId(2B), data...]
uint8_t headerByte= SPINEL_HEADER_FLAG | (2 &0x0F); // TID=2?

std::vector<uint8_t> spinel;
spinel.push_back(headerByte);

spinel.push_back((uint8_t)(SPINEL_CMD_PROP_VALUE_SET &0xFF));
spinel.push_back((uint8_t)((SPINEL_CMD_PROP_VALUE_SET>>8)&0xFF));

spinel.push_back((uint8_t)(propId &0xFF));
spinel.push_back((uint8_t)((propId>>8)&0xFF));

spinel.insert(spinel.end(), data.begin(), data.end());

return sendSpinelFrame(spinel);
}

/********************************************************************
 * rxLoop
 *   - read from SerialPort, feed HDLC decode,
 *   - handle completed spinel frames
 ********************************************************************/
void RawSpinelDriver::rxLoop()
{
    std::vector<uint8_t> buffer; // accumulate partial data for HDLC
    buffer.reserve(1024);

    while(m_running)
    {
        auto data = m_port.readSome(); // blocking or short-timeout read
        if(!data.empty())
        {
            // accumulate & parse
            buffer.insert(buffer.end(), data.begin(), data.end());
            // try decode one or more frames
            size_t pos=0;
            while(pos<buffer.size())
            {
                // look for 0x7E flags
                auto flagPos= std::find(buffer.begin()+pos, buffer.end(), HDLC_FLAG);
                if(flagPos==buffer.end())
                    break; // no more frames
                // we see a start of frame => see if we can find the next 0x7E
                auto nextFlag= std::find(flagPos+1, buffer.end(), HDLC_FLAG);
                if(nextFlag==buffer.end())
                {
                    // we have start but no end => wait for more data
                    pos= (flagPos-buffer.begin());
                    break;
                }
                // we have a complete HDLC frame from flagPos.. nextFlag
                std::vector<uint8_t> hdlcFrame(flagPos+1, nextFlag); // exclude flags
                // decode
                std::vector<uint8_t> spinelFrame;
                if(hdlcDecode(hdlcFrame, spinelFrame))
                {
                    handleSpinelFrame(spinelFrame);
                }
                pos= (nextFlag - buffer.begin())+1;
            }
            // remove processed data from buffer
            if(pos>0)
            {
                buffer.erase(buffer.begin(), buffer.begin()+pos);
            }
        }
        else
        {
            // no data => sleep a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

/********************************************************************
 * sendSpinelFrame
 *   - HDLC-encode + send over serial
 ********************************************************************/
bool RawSpinelDriver::sendSpinelFrame(const std::vector<uint8_t> &spinelPayload)
{
    std::lock_guard<std::mutex> lock(m_sendMutex);

    auto hdlcFrame = hdlcEncode(spinelPayload.data(), spinelPayload.size());
    return m_port.writeBytes(hdlcFrame);
}

/********************************************************************
 * handleSpinelFrame
 *   - parse [header(1B), cmd(2B), propId(2B), payload...]
 *   - if propId==SPINEL_PROP_STREAM_RAW => call rawFrameHandler
 *   - else => call spinelEventHandler
 ********************************************************************/
void RawSpinelDriver::handleSpinelFrame(const std::vector<uint8_t> &spinelFrame)
{
    if(spinelFrame.size()<5)
    {
        std::cerr<<"[RawSpinel] Frame too short\n";
        return;
    }
    uint8_t header= spinelFrame[0];
    uint16_t cmd  = spinelFrame[1]|(spinelFrame[2]<<8);
    uint16_t prop = spinelFrame[3]|(spinelFrame[4]<<8);

    std::vector<uint8_t> payload;
    if(spinelFrame.size()>5)
    {
        payload.assign(spinelFrame.begin()+5, spinelFrame.end());
    }

    if(cmd== (uint16_t)SpinelCommand::SPINEL_CMD_PROP_VALUE_IS)
    {
        if(prop== SPINEL_PROP_STREAM_RAW)
        {
            // raw 802.15.4 frame inbound
            if(m_rawFrameHandler)
            {
                m_rawFrameHandler(payload);
            }
        }
        else
        {
            // some other property => call event handler
            if(m_spinelEventHandler)
            {
                m_spinelEventHandler(prop, payload);
            }
        }
    }
    else
    {
        // possibly a ack or response to a setProp => we can parse further if we track TIDs
        // for now, just log
        std::cout<<"[RawSpinel] cmd="<<cmd<< " prop="<<prop<<" unhandled\n";
    }
}

/********************************************************************
 * HDLC encode / decode
 ********************************************************************/

// Compute 16-bit CRC-CCITT
static uint16_t crc16Ccitt(const uint8_t* data, size_t length, uint16_t init=0xFFFF)
{
    uint16_t crc= init;
    for(size_t i=0;i<length;i++)
    {
        crc ^= (data[i]<<8);
        for(int b=0;b<8;b++)
        {
            if(crc&0x8000)
                crc= (crc<<1)^0x1021;
            else
                crc= crc<<1;
        }
    }
    return crc;
}

// HDLC encode with start/end flag=0x7E, escape 0x7E/0x7D, append 16-bit CRC
std::vector<uint8_t> RawSpinelDriver::hdlcEncode(const uint8_t* data, size_t len)
{
    std::vector<uint8_t> out;
    out.reserve(len*2+6);

    // Start flag
    out.push_back(HDLC_FLAG);

    // compute crc
    uint16_t crc= crc16Ccitt(data, len);

    // payload + CRC
    std::vector<uint8_t> payload(data, data+len);
    payload.push_back(crc &0xFF);
    payload.push_back((crc>>8)&0xFF);

    // escape + stuff
    for(auto b: payload)
    {
        if(b==HDLC_FLAG || b==HDLC_ESCAPE)
        {
            out.push_back(HDLC_ESCAPE);
            out.push_back(b ^ HDLC_XOR);
        }
        else
        {
            out.push_back(b);
        }
    }

    // end flag
    out.push_back(HDLC_FLAG);

    return out;
}

// HDLC decode. We assume “input” is the frame excluding 0x7E flags at each end, but we escape/unescape & check CRC
bool RawSpinelDriver::hdlcDecode(const std::vector<uint8_t> &input,
                                 std::vector<uint8_t> &output)
{
    // unescape
    std::vector<uint8_t> unescaped;
    unescaped.reserve(input.size());

    bool escaping=false;
    for(auto b: input)
    {
        if(escaping)
        {
            unescaped.push_back(b ^ HDLC_XOR);
            escaping=false;
        }
        else if(b==HDLC_ESCAPE)
        {
            escaping=true;
        }
        else
        {
            unescaped.push_back(b);
        }
    }

    if(unescaped.size()<2)
        return false;

    // check CRC
    size_t dataLen= unescaped.size()-2;
    uint8_t crcLo= unescaped[dataLen];
    uint8_t crcHi= unescaped[dataLen+1];
    uint16_t frameCrc= (crcHi<<8)|crcLo;

    std::vector<uint8_t> rawData(unescaped.begin(), unescaped.begin()+ dataLen);

    uint16_t calcCrc= crc16Ccitt(rawData.data(), rawData.size());
    if(calcCrc != frameCrc)
    {
        std::cerr<<"[RawSpinel] HDLC CRC error\n";
        return false;
    }
    output= rawData;
    return true;
}
