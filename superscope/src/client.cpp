#include "client.h"
#include "network/framing.h"
#include "network/messages.h"
#include "network/socket.h"
#include <arpa/inet.h>
#include <cstring>

namespace ss {

Client::Client(NodeId client_id)
    : m_id(client_id)
    , m_disc(client_id, {.ip = "127.0.0.1", .port = 0}, {.ip = "127.0.0.1", .port = 0}, 0) {}

std::optional<LeaderInfo> Client::cached_leader() const {
    return m_leader;
}

void Client::clear_leader() {
    m_leader.reset();
}

std::optional<LeaderInfo> Client::discover_leader() {
    clear_leader();
    m_disc.broadcast_who_is_leader();

    for (int i = 0; i < 3 && !m_leader; ++i) {
        m_disc.poll_once(
            nullptr,
            nullptr,
            [this](NodeId lid, const Endpoint& ep) {
                LeaderInfo info = {
                    .id = lid,
                    .ep = ep,
                };
                m_leader = info;
            },
            nullptr,
            500);
    }
    return m_leader;
}

bool Client::ensure_leader() {
    if (!m_leader) discover_leader();
    return m_leader.has_value();
}

std::optional<SwarmInfo> Client::query_swarm() {
    if (!ensure_leader()) return std::nullopt;

    try {
        Socket conn = Socket::tcp_client(m_leader->ep.ip, m_leader->ep.port);
        PacketHeader h = {
            .magic       = {'S', 'S'},
            .type        = MsgType::SwarmQuery,
            ._pad        = 0,
            .sender_id   = m_id,
            .payload_len = 0,
        };
        if (!send_framed(conn.fd(), encode_header(h))) return std::nullopt;

        std::vector<uint8_t> resp;
        if (!recv_framed(conn.fd(), resp)) return std::nullopt;

        PacketHeader rh{};
        if (!decode_header(resp.data(), resp.size(), rh) ||
            rh.type != MsgType::SwarmStatus ||
            rh.payload_len < sizeof(SwarmStatusPayload)) {
            return std::nullopt;
        }

        const uint8_t* pl = resp.data() + sizeof(PacketHeader);
        SwarmStatusPayload st{};
        std::memcpy(&st, pl, sizeof(st));
        uint32_t sz = ntohl(st.size);

        SwarmInfo info = {
            .size  = sz,
            .nodes = {},
        };
        const uint8_t* id_ptr = pl + sizeof(st);
        for (uint32_t i = 0; i < sz && (sizeof(st) + (i + 1) * 4) <= rh.payload_len; ++i) {
            uint32_t be;
            std::memcpy(&be, id_ptr + i * 4, 4);
            info.nodes.push_back(ntohl(be));
        }
        return info;
    } catch (...) {
        clear_leader();
        return std::nullopt;
    }
}

std::optional<QueueInfo> Client::query_queue() {
    if (!ensure_leader()) return std::nullopt;

    try {
        Socket conn = Socket::tcp_client(m_leader->ep.ip, m_leader->ep.port);
        PacketHeader h = {
            .magic       = {'S', 'S'},
            .type        = MsgType::QueueQuery,
            ._pad        = 0,
            .sender_id   = m_id,
            .payload_len = 0,
        };
        if (!send_framed(conn.fd(), encode_header(h))) return std::nullopt;

        std::vector<uint8_t> resp;
        if (!recv_framed(conn.fd(), resp)) return std::nullopt;

        PacketHeader rh{};
        if (!decode_header(resp.data(), resp.size(), rh) ||
            rh.type != MsgType::QueueStatus ||
            rh.payload_len < sizeof(QueueStatusPayload)) {
            return std::nullopt;
        }

        QueueStatusPayload st{};
        std::memcpy(&st, resp.data() + sizeof(PacketHeader), sizeof(st));
        QueueInfo info = {
            .position = ntohl(st.position),
            .total    = ntohl(st.total),
        };
        return info;
    } catch (...) {
        clear_leader();
        return std::nullopt;
    }
}

std::optional<std::vector<uint8_t>> Client::request_capture() {
    if (!ensure_leader()) return std::nullopt;

    try {
        Socket conn = Socket::tcp_client(m_leader->ep.ip, m_leader->ep.port);
        std::string job = R"({"type":"panorama"})";
        PacketHeader h = {
            .magic       = {'S', 'S'},
            .type        = MsgType::JobRequest,
            ._pad        = 0,
            .sender_id   = m_id,
            .payload_len = (uint32_t)job.size(),
        };
        auto pkt = encode_header(h);
        pkt.insert(pkt.end(), job.begin(), job.end());

        if (!send_framed(conn.fd(), pkt)) return std::nullopt;

        std::vector<uint8_t> resp;
        if (!recv_framed(conn.fd(), resp)) return std::nullopt;
        return resp;
    } catch (...) {
        clear_leader();
        return std::nullopt;
    }
}

} // namespace ss
