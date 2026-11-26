#include "network/framing.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>

namespace ss {

std::vector<uint8_t> frame(const std::vector<uint8_t>& payload) {
    uint32_t len_be = htonl(static_cast<uint32_t>(payload.size()));
    std::vector<uint8_t> out;
    out.reserve(4 + payload.size());
    const uint8_t* lp = reinterpret_cast<const uint8_t*>(&len_be);
    out.insert(out.end(), lp, lp + 4);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

void Unframer::append(const uint8_t* data, size_t len) {
    m_buf.insert(m_buf.end(), data, data + len);
}

bool Unframer::pop(std::vector<uint8_t>& out) {
    if (m_buf.size() < 4) return false;
    uint32_t len_be;
    std::memcpy(&len_be, m_buf.data(), 4);
    uint32_t n = ntohl(len_be);
    if (m_buf.size() < 4 + n) return false;
    out.assign(m_buf.begin() + 4, m_buf.begin() + 4 + n);
    m_buf.erase(m_buf.begin(), m_buf.begin() + 4 + n);
    return true;
}

bool send_all(int fd, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool recv_all(int fd, uint8_t* data, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, data + got, len - got, 0);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

bool send_framed(int fd, const std::vector<uint8_t>& payload) {
    auto f = frame(payload);
    return send_all(fd, f.data(), f.size());
}

bool recv_framed(int fd, std::vector<uint8_t>& out) {
    uint32_t len_be = 0;
    if (!recv_all(fd, reinterpret_cast<uint8_t*>(&len_be), 4)) return false;
    uint32_t n = ntohl(len_be);
    out.resize(n);
    return recv_all(fd, out.data(), n);
}

} // namespace ss
