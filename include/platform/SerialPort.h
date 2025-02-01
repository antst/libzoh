#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum class FlowControl
{
    None,
    Software, // XON/XOFF
    Hardware  // RTS/CTS
};

class SerialPort
{
public:
    // If nonBlocking == true, read/write calls return immediately if they cannot proceed.
    // If false, they block until data is read/written or a timeout occurs.
    SerialPort(const std::string &portName,
               int baudRate,
               FlowControl flowControl = FlowControl::None,
               bool nonBlocking = false);

    ~SerialPort();

    // Non-blocking write:
    //  - On POSIX, writes as many bytes as possible immediately.
    //  - On Windows, attempts an overlapped write; if it would block, returns partial or 0.
    // Returns the number of bytes actually written (which may be < data.size()).
    // Returns -1 on error.
    int writeBytes(const std::vector<uint8_t> &data);

    // Non-blocking read:
    //  - On POSIX, if no data is available, returns 0 immediately (errno=EAGAIN internally).
    //  - On Windows, attempts overlapped read; if no data is available, returns 0 immediately.
    // Returns a vector of the bytes that were read (could be empty).
    // On error, returns an empty vector and you may check logs or debug.
    std::vector<uint8_t> readSome(size_t maxBytesToRead = 1024);

private:
    // Disallow copy
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    void configureFlowControl(FlowControl flow);

#ifdef _WIN32
    void*      m_handle;      // HANDLE for Windows
    // We store an OVERLAPPED structure for async read/write.
    // In a more robust design, you might keep separate ones for read vs write.
    // Or you might keep a persistent read overlapped posted to detect incoming data.
    // Here, we just create them on the fly for simplicity.
#else
    int        m_fd;          // file descriptor for POSIX
#endif

    FlowControl  m_flowControl;
    bool         m_nonBlocking;
};
