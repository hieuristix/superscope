#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ss {

// prepend a 4-byte big-endian length prefix to a payload
std::vector<uint8_t> frame(const std::vector<uint8_t>& payload);

// accumulates streamed tcp bytes and extracts complete length-prefixed frames
struct Unframer {
    // append newly received bytes into the buffer
    void append(const uint8_t* data, size_t len);

    // extract the next complete frame if enough bytes are buffered
    bool pop(std::vector<uint8_t>& out);

private:
    // internal byte accumulation buffer
    std::vector<uint8_t> m_buf;
};

// send exactly `len` bytes over a blocking tcp socket
bool send_all(int fd, const uint8_t* data, size_t len);

// read exactly `len` bytes from a blocking tcp socket
bool recv_all(int fd, uint8_t* data, size_t len);

// length-prefix and send one complete message over tcp
bool send_framed(int fd, const std::vector<uint8_t>& payload);

// block until one complete length-prefixed message is received over tcp
bool recv_framed(int fd, std::vector<uint8_t>& out);

} // namespace ss
