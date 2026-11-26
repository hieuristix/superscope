#pragma once
#include "network/types.h"
#include <string>

namespace ss {

// move-only raii wrapper around a posix socket file descriptor
struct Socket {
    // construct an invalid empty socket
    Socket() = default;

    // take ownership of an existing file descriptor
    explicit Socket(int fd);

    // close the owned descriptor if valid
    ~Socket();

    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;

    // transfer descriptor ownership from another socket
    Socket(Socket&&) noexcept;

    // replace owned descriptor with another socket's descriptor
    Socket& operator=(Socket&&) noexcept;

    // return the underlying posix file descriptor
    int fd() const { return m_fd; }

    // return true if the socket holds an open descriptor
    bool valid() const { return m_fd >= 0; }

    // close the socket early and mark it invalid
    void close();

    // create a bound and listening tcp server socket
    static Socket tcp_server(uint16_t port, int backlog = 10);

    // create a tcp client socket connected to ip:port
    static Socket tcp_client(const std::string& ip, uint16_t port);

    // create an unbound udp socket for sending
    static Socket udp_sender();

    // create a udp socket with SO_BROADCAST enabled
    static Socket udp_broadcast();

    // create a udp socket bound to port
    static Socket udp_listener(uint16_t port, bool broadcast = false);

    // create a udp socket configured for sending multicast packets
    static Socket mcast_sender(const std::string& group, uint8_t ttl = 1);

    // create a udp socket bound to port and joined to a multicast group
    static Socket mcast_recv(const std::string& group, uint16_t port);

private:
    // owned posix socket descriptor (-1 when closed)
    int m_fd{-1};
};

// detect the first non-loopback ipv4 address on this host, or fallback to 127.0.0.1
std::string detect_local_ipv4();

} // namespace ss
