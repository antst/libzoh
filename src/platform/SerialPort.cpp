#include "platform/SerialPort.h"

#include <stdexcept>
#include <iostream>
#include <cstring>

// Platform-specific
#ifdef _WIN32
#include <windows.h>
#else

#include <fcntl.h>      // open, O_RDWR, etc.
#include <unistd.h>     // read, write, close
#include <termios.h>    // struct termios, tcgetattr, cfsetispeed, etc.
#include <errno.h>

#ifdef __linux__
#include <sys/ioctl.h>
#endif
#endif

static void logError(const char *msg) {
    std::cerr << "[SerialPort] Error: " << msg << std::endl;
}

// Helper (POSIX) to convert baud rate to speed_t
#ifndef _WIN32

static speed_t getBaudConstant(int baudRate) {
    switch (baudRate) {
        case 50:
            return B50;
        case 75:
            return B75;
        case 110:
            return B110;
        case 134:
            return B134;
        case 150:
            return B150;
        case 200:
            return B200;
        case 300:
            return B300;
        case 600:
            return B600;
        case 1200:
            return B1200;
        case 1800:
            return B1800;
        case 2400:
            return B2400;
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        default:
            return B115200;
    }
}

#endif

SerialPort::SerialPort(const std::string &portName,
                       int baudRate,
                       FlowControl flowControl,
                       bool nonBlocking)
        : m_flowControl(flowControl), m_nonBlocking(nonBlocking) {
#ifdef _WIN32
    m_handle = INVALID_HANDLE_VALUE;

    // Convert to wide string
    std::wstring wportName(portName.begin(), portName.end());

    DWORD dwFlags = 0;
    if (m_nonBlocking)
    {
        // Overlapped for "async" non-blocking
        dwFlags |= FILE_FLAG_OVERLAPPED;
    }

    m_handle = CreateFileW(
        wportName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, // exclusive
        NULL,
        OPEN_EXISTING,
        dwFlags,
        NULL
    );

    if (m_handle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Failed to open serial port: " + portName);
    }

    // Basic settings (8N1). We'll set flow control next
    // Windows "non-blocking" still typically uses timeouts if we do synchronous I/O,
    // but overlapped means we won't block in ReadFile/WriteFile if we handle it carefully.

    DCB dcbConfig;
    ZeroMemory(&dcbConfig, sizeof(dcbConfig));
    dcbConfig.DCBlength = sizeof(dcbConfig);

    if (!GetCommState(m_handle, &dcbConfig))
    {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
        throw std::runtime_error("GetCommState failed");
    }

    dcbConfig.BaudRate = baudRate;
    dcbConfig.ByteSize = 8;
    dcbConfig.Parity   = NOPARITY;
    dcbConfig.StopBits = ONESTOPBIT;

    // Turn off all flow control bits initially
    dcbConfig.fOutxCtsFlow = FALSE;
    dcbConfig.fRtsControl  = RTS_CONTROL_DISABLE;
    dcbConfig.fOutX        = FALSE;
    dcbConfig.fInX         = FALSE;

    if (!SetCommState(m_handle, &dcbConfig))
    {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
        throw std::runtime_error("SetCommState failed");
    }

    // Setup timeouts to 0 if we truly want immediate returns (for overlapped, these timeouts are less relevant).
    COMMTIMEOUTS timeouts;
    ZeroMemory(&timeouts, sizeof(timeouts));
    // No blocking
    timeouts.ReadIntervalTimeout         = 0;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.WriteTotalTimeoutConstant   = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(m_handle, &timeouts);

    // Flow control
    configureFlowControl(m_flowControl);

#else
    // POSIX
    int flags = O_RDWR | O_NOCTTY;
    if (m_nonBlocking) {
        flags |= O_NONBLOCK;  // non-blocking
    }

    m_fd = ::open(portName.c_str(), flags);
    if (m_fd < 0) {
        throw std::runtime_error("Failed to open serial port: " + portName +
                                 " error: " + std::strerror(errno));
    }

#ifdef __linux__
    // TIOCEXCL
    if (ioctl(m_fd, TIOCEXCL) != 0)
    {
        // not fatal
        std::cerr << "[SerialPort] Warning: TIOCEXCL failed.\n";
    }
#endif

    // Basic config
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(m_fd, &tty) != 0) {
        ::close(m_fd);
        throw std::runtime_error("Failed to get port attrs: " +
                                 std::string(std::strerror(errno)));
    }

    speed_t speed = getBaudConstant(baudRate);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= (CLOCAL | CREAD);

    // No canonical, no echo
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;

    // Flow control off by default
    tty.c_cflag &= ~CRTSCTS;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // If in blocking mode, set VMIN/VTIME. In non-blocking, we can ignore them or set to 0
    if (!m_nonBlocking) {
        tty.c_cc[VMIN] = 1;  // wait for 1 byte
        tty.c_cc[VTIME] = 1;  // tenth of a second
    } else {
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;
    }

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        ::close(m_fd);
        throw std::runtime_error("Failed to set port attrs: " +
                                 std::string(std::strerror(errno)));
    }

    configureFlowControl(m_flowControl);

#endif // _WIN32
}

SerialPort::~SerialPort() {
#ifdef _WIN32
    if (m_handle && m_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

void SerialPort::configureFlowControl(FlowControl flow) {
#ifdef _WIN32
    if (!m_handle || m_handle == INVALID_HANDLE_VALUE)
        return;

    DCB dcbConfig;
    ZeroMemory(&dcbConfig, sizeof(dcbConfig));
    dcbConfig.DCBlength = sizeof(dcbConfig);

    if (!GetCommState(m_handle, &dcbConfig))
    {
        std::cerr << "[SerialPort] Warning: GetCommState failed\n";
        return;
    }

    // Clear flow bits
    dcbConfig.fOutxCtsFlow = FALSE;
    dcbConfig.fRtsControl  = RTS_CONTROL_DISABLE;
    dcbConfig.fOutX        = FALSE;
    dcbConfig.fInX         = FALSE;

    if (flow == FlowControl::Hardware)
    {
        dcbConfig.fOutxCtsFlow = TRUE;
        dcbConfig.fRtsControl  = RTS_CONTROL_HANDSHAKE;
    }
    else if (flow == FlowControl::Software)
    {
        dcbConfig.fOutX = TRUE;
        dcbConfig.fInX  = TRUE;
    }

    if (!SetCommState(m_handle, &dcbConfig))
    {
        std::cerr << "[SerialPort] Warning: SetCommState failed\n";
    }

#else
    if (m_fd < 0)
        return;

    struct termios tty;
    if (tcgetattr(m_fd, &tty) != 0) {
        std::cerr << "[SerialPort] Warning: tcgetattr failed in configureFlowControl\n";
        return;
    }

    // Clear flow bits
    tty.c_cflag &= ~CRTSCTS;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    if (flow == FlowControl::Hardware) {
        tty.c_cflag |= CRTSCTS;
    } else if (flow == FlowControl::Software) {
        tty.c_iflag |= (IXON | IXOFF);
    }

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        std::cerr << "[SerialPort] Warning: flow control set failed\n";
    }
#endif
}

// Non-blocking write
int SerialPort::writeBytes(const std::vector<uint8_t> &data) {
    if (data.empty())
        return 0;

#ifdef _WIN32
    if (!m_handle || m_handle == INVALID_HANDLE_VALUE)
    {
        logError("writeBytes: Invalid handle");
        return -1;
    }

    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof(ov));
    // Create an event for overlapped operation
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent)
    {
        logError("writeBytes: CreateEvent failed");
        return -1;
    }

    DWORD bytesWritten = 0;
    BOOL success = WriteFile(m_handle,
                             data.data(),
                             (DWORD)data.size(),
                             &bytesWritten,
                             &ov);
    if (!success)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            // Operation would block. We do NOT wait. We return 0 or partial success.
            // If you wanted truly async, you'd keep track of 'ov' and complete later.
            CancelIoEx(m_handle, &ov); // Cancel so we don't leave it hanging
            CloseHandle(ov.hEvent);
            return 0; // means "would block"
        }
        else
        {
            logError("writeBytes: WriteFile failed");
            CloseHandle(ov.hEvent);
            return -1;
        }
    }
    else
    {
        // If WriteFile completed immediately
        // 'bytesWritten' tells how many were written
        CloseHandle(ov.hEvent);
        return (int)bytesWritten;
    }

#else
    if (m_fd < 0) {
        logError("writeBytes: Invalid fd");
        return -1;
    }
    ssize_t result = ::write(m_fd, data.data(), data.size());
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // would block
            return 0;
        } else {
            logError(std::strerror(errno));
            return -1;
        }
    }
    return (int) result;
#endif
}

// Non-blocking read
std::vector<uint8_t> SerialPort::readSome(size_t maxBytesToRead) {
    if (maxBytesToRead == 0)
        return {};

    std::vector<uint8_t> buffer(maxBytesToRead, 0);

#ifdef _WIN32
    if (!m_handle || m_handle == INVALID_HANDLE_VALUE)
    {
        logError("readSome: Invalid handle");
        return {};
    }

    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof(ov));
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent)
    {
        logError("readSome: CreateEvent failed");
        return {};
    }

    DWORD bytesRead = 0;
    BOOL success = ReadFile(m_handle,
                            buffer.data(),
                            (DWORD)buffer.size(),
                            &bytesRead,
                            &ov);
    if (!success)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            // means there's no data right now -> would block
            CancelIoEx(m_handle, &ov); // cancel the pending read
            CloseHandle(ov.hEvent);
            return {}; // empty result
        }
        else
        {
            logError("readSome: ReadFile failed");
            CloseHandle(ov.hEvent);
            return {};
        }
    }
    else
    {
        // Completed immediately, bytesRead is valid
        CloseHandle(ov.hEvent);
        buffer.resize(bytesRead);
        return buffer;
    }

#else
    if (m_fd < 0) {
        logError("readSome: Invalid fd");
        return {};
    }

    ssize_t n = ::read(m_fd, buffer.data(), buffer.size());
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data available
            return {};
        } else {
            logError(std::strerror(errno));
            return {};
        }
    }
    buffer.resize((size_t) n);
    return buffer;
#endif
}
