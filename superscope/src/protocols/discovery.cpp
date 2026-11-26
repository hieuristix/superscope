#include "protocols/discovery.h"
#include "network/messages.h"
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

namespace ss {

static constexpr size_t kBuf = 4096;

static Socket make_discovery_socket(uint16_t port) {
    Socket s = (port == 0)
        ? Socket::udp_listener(0, true)
        : Socket::mcast_recv(kMulticastGroup, port);
    int on = 1;
    uint8_t ttl = 1;
    setsockopt(s.fd(), SOL_SOCKET, SO_BROADCAST,      &on,  sizeof(on));
    setsockopt(s.fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &on,  sizeof(on));
    setsockopt(s.fd(), IPPROTO_IP, IP_MULTICAST_TTL,  &ttl, sizeof(ttl));
    in_addr iface{};
    inet_pton(AF_INET, detect_local_ipv4().c_str(), &iface);
    setsockopt(s.fd(), IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof(iface));
    return s;
}

Discovery::Discovery(NodeId id, const Endpoint& tcp, const Endpoint& hb, uint16_t listen_port)
    : m_id(id), m_tcp(tcp), m_hb(hb), m_sock(make_discovery_socket(listen_port)) {}

static void udp_send(Socket& sock, const std::vector<uint8_t>& data, const std::string& ip, uint16_t port) {
    sockaddr_in d{}; d.sin_family = AF_INET; d.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &d.sin_addr);
    sendto(sock.fd(), data.data(), data.size(), 0, (sockaddr*)&d, sizeof(d));
}

static std::vector<uint8_t> make_pkt(MsgType type, NodeId sender, const void* p, size_t plen) {
    PacketHeader h = {
        .magic       = {'S', 'S'},
        .type        = type,
        ._pad        = 0,
        .sender_id   = sender,
        .payload_len = (uint32_t)plen,
    };
    auto hb = encode_header(h);
    std::vector<uint8_t> out = hb;
    if (p && plen) out.insert(out.end(), (const uint8_t*)p, (const uint8_t*)p + plen);
    return out;
}

static std::vector<uint8_t> encode_view_payload(const std::vector<NodeInfo>& members) {
    std::vector<uint8_t> payload;
    for (const auto& m : members) {
        SwarmEntry e = {
            .node_id  = htonl(m.id),
            .tcp_ip   = {},
            .tcp_port = htons(m.tcp.port),
            .hb_ip    = {},
            .hb_port  = htons(m.hb.port),
        };
        std::strncpy(e.tcp_ip, m.tcp.ip.c_str(), 15);
        std::strncpy(e.hb_ip,  m.hb.ip.c_str(),  15);
        payload.insert(payload.end(), (uint8_t*)&e, (uint8_t*)&e + sizeof(e));
    }
    return payload;
}

static void udp_broadcast_both(Socket& sock, const std::vector<uint8_t>& pkt) {
    udp_send(sock, pkt, kMulticastGroup, kDiscoveryPort);
    udp_send(sock, pkt, kBroadcastAddr,  kDiscoveryPort);
}

void Discovery::broadcast_join() {
    JoinPayload p = {
        .node_id  = htonl(m_id),
        .tcp_ip   = {},
        .tcp_port = htons(m_tcp.port),
        .hb_ip    = {},
        .hb_port  = htons(m_hb.port),
    };
    std::strncpy(p.tcp_ip, m_tcp.ip.c_str(), 15);
    std::strncpy(p.hb_ip,  m_hb.ip.c_str(),  15);
    udp_broadcast_both(m_sock, make_pkt(MsgType::DiscoveryJoin, m_id, &p, sizeof(p)));
}

void Discovery::send_view(const Endpoint& target, const std::vector<NodeInfo>& members) {
    auto payload = encode_view_payload(members);
    udp_send(m_sock, make_pkt(MsgType::SwarmView, m_id, payload.data(), payload.size()), target.ip, target.port);
}

void Discovery::broadcast_view(const std::vector<NodeInfo>& members) {
    auto payload = encode_view_payload(members);
    udp_broadcast_both(m_sock, make_pkt(MsgType::SwarmView, m_id, payload.data(), payload.size()));
}

void Discovery::broadcast_who_is_leader() {
    udp_broadcast_both(m_sock, make_pkt(MsgType::WhoIsLeader, m_id, nullptr, 0));
}

void Discovery::reply_leader(const Endpoint& target, const Endpoint& leader_tcp) {
    JoinPayload p = {
        .node_id  = htonl(m_id),
        .tcp_ip   = {},
        .tcp_port = htons(leader_tcp.port),
        .hb_ip    = {},
        .hb_port  = 0,
    };
    std::strncpy(p.tcp_ip, leader_tcp.ip.c_str(), 15);
    udp_send(m_sock, make_pkt(MsgType::LeaderAnnounce, m_id, &p, sizeof(p)), target.ip, target.port);
}

bool Discovery::poll_once(OnJoin on_join, OnView on_view, OnLeader on_leader,
                          OnWhoIsLeader on_who, int timeout_ms) {
    if (timeout_ms >= 0) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(m_sock.fd(), &rfds);
        timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        if (select(m_sock.fd() + 1, &rfds, nullptr, nullptr, &tv) <= 0) return false;
    }

    uint8_t buf[kBuf];
    sockaddr_in sa{}; socklen_t sl = sizeof(sa);
    ssize_t n = recvfrom(m_sock.fd(), buf, sizeof(buf), 0, (sockaddr*)&sa, &sl);
    if (n < (ssize_t)sizeof(PacketHeader)) return false;
    PacketHeader h{};
    if (!decode_header(buf, (size_t)n, h)) return false;
    if (h.sender_id == m_id) return false;

    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof(ip));
    Endpoint sender_ep = {
        .ip   = ip,
        .port = ntohs(sa.sin_port),
    };

    const uint8_t* pl = buf + sizeof(PacketHeader);

    if (h.type == MsgType::DiscoveryJoin && h.payload_len >= sizeof(JoinPayload)) {
        JoinPayload p{}; std::memcpy(&p, pl, sizeof(p));
        NodeInfo info = {
            .id  = ntohl(p.node_id),
            .tcp = {.ip = p.tcp_ip, .port = ntohs(p.tcp_port)},
            .hb  = {.ip = p.hb_ip,  .port = ntohs(p.hb_port)},
        };
        if (on_join) on_join(info, sender_ep);

    } else if (h.type == MsgType::SwarmView) {
        size_t cnt = h.payload_len / sizeof(SwarmEntry);
        std::vector<NodeInfo> members;
        for (size_t i = 0; i < cnt; ++i) {
            SwarmEntry e{}; std::memcpy(&e, pl + i*sizeof(e), sizeof(e));
            NodeInfo info = {
                .id  = ntohl(e.node_id),
                .tcp = {.ip = e.tcp_ip, .port = ntohs(e.tcp_port)},
                .hb  = {.ip = e.hb_ip,  .port = ntohs(e.hb_port)},
            };
            members.push_back(info);
        }
        if (on_view) on_view(members);

    } else if (h.type == MsgType::WhoIsLeader) {
        if (on_who) on_who(sender_ep);

    } else if (h.type == MsgType::LeaderAnnounce && h.payload_len >= sizeof(JoinPayload)) {
        JoinPayload p{}; std::memcpy(&p, pl, sizeof(p));
        Endpoint leader_ep = {
            .ip   = p.tcp_ip,
            .port = ntohs(p.tcp_port),
        };
        if (on_leader) on_leader(ntohl(p.node_id), leader_ep);
    }
    return true;
}

} // namespace ss
