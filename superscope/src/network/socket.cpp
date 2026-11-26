#include "network/socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>

namespace ss {

Socket::Socket(int fd) : m_fd(fd) {}
Socket::~Socket() { close(); }
Socket::Socket(Socket&& o) noexcept : m_fd(o.m_fd) { o.m_fd = -1; }
Socket& Socket::operator=(Socket&& o) noexcept {
    if (this != &o) { close(); m_fd = o.m_fd; o.m_fd = -1; }
    return *this;
}
void Socket::close() { if (m_fd >= 0) { ::close(m_fd); m_fd = -1; } }

static void set_opt(int fd, int lvl, int opt, int val = 1) {
    setsockopt(fd, lvl, opt, &val, sizeof(val));
}

Socket Socket::tcp_server(uint16_t port, int backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    set_opt(fd, SOL_SOCKET, SO_REUSEADDR);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(port);
    if (bind(fd, (sockaddr*)&a, sizeof(a)) < 0) throw std::runtime_error("bind failed port " + std::to_string(port));
    if (listen(fd, backlog) < 0) throw std::runtime_error("listen failed");
    return Socket(fd);
}

Socket Socket::tcp_client(const std::string& ip, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &a.sin_addr);
    if (connect(fd, (sockaddr*)&a, sizeof(a)) < 0) throw std::runtime_error("connect failed to " + ip);
    return Socket(fd);
}

Socket Socket::udp_sender() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    return Socket(fd);
}

Socket Socket::udp_broadcast() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    set_opt(fd, SOL_SOCKET, SO_BROADCAST);
    return Socket(fd);
}

Socket Socket::udp_listener(uint16_t port, bool broadcast) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    set_opt(fd, SOL_SOCKET, SO_REUSEADDR);
#ifdef SO_REUSEPORT
    set_opt(fd, SOL_SOCKET, SO_REUSEPORT);
#endif
    if (broadcast) set_opt(fd, SOL_SOCKET, SO_BROADCAST);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(port);
    if (bind(fd, (sockaddr*)&a, sizeof(a)) < 0) throw std::runtime_error("bind failed port " + std::to_string(port));
    return Socket(fd);
}

std::string detect_local_ipv4() {
    ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return "127.0.0.1";
    std::string result = "127.0.0.1";
    for (ifaddrs* cur = ifa; cur; cur = cur->ifa_next) {
        if (!cur->ifa_addr || cur->ifa_addr->sa_family != AF_INET) continue;
        if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK)) continue;
        char buf[INET_ADDRSTRLEN]{};
        auto* sin = reinterpret_cast<sockaddr_in*>(cur->ifa_addr);
        if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
            result = buf;
            break;
        }
    }
    freeifaddrs(ifa);
    return result;
}

Socket Socket::mcast_sender(const std::string&, uint8_t ttl) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    set_opt(fd, IPPROTO_IP, IP_MULTICAST_LOOP);
    in_addr iface{};
    inet_pton(AF_INET, detect_local_ipv4().c_str(), &iface);
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof(iface));
    return Socket(fd);
}

Socket Socket::mcast_recv(const std::string& group, uint16_t port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    set_opt(fd, SOL_SOCKET, SO_REUSEADDR);
#ifdef SO_REUSEPORT
    set_opt(fd, SOL_SOCKET, SO_REUSEPORT);
#endif
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(port);
    if (bind(fd, (sockaddr*)&a, sizeof(a)) < 0)
        throw std::runtime_error("bind failed port " + std::to_string(port));
    ip_mreq m{};
    inet_pton(AF_INET, group.c_str(), &m.imr_multiaddr);
    inet_pton(AF_INET, detect_local_ipv4().c_str(), &m.imr_interface);
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof(m)) < 0) {
        m.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof(m)) < 0)
            throw std::runtime_error("IP_ADD_MEMBERSHIP failed");
    }
    return Socket(fd);
}

} // namespace ss
