#include "protocols/failure.h"
#include "network/messages.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/select.h>
#include <sys/socket.h>

namespace ss {

namespace {

std::vector<uint8_t> make_heartbeat(NodeId id) {
    HbPayload payload{.sender_id = htonl(id)};
    PacketHeader header = {
        .magic = {'S', 'S'},
        .type = MsgType::Heartbeat,
        ._pad = 0,
        .sender_id = id,
        .payload_len = sizeof(payload),
    };
    auto packet = encode_header(header);
    const auto* bytes = reinterpret_cast<const uint8_t*>(&payload);
    packet.insert(packet.end(), bytes, bytes + sizeof(payload));
    return packet;
}

bool parse_heartbeat(const uint8_t* data, size_t length, NodeId& sender) {
    PacketHeader header{};
    if (!decode_header(data, length, header) ||
        header.type != MsgType::Heartbeat ||
        header.payload_len != sizeof(HbPayload) ||
        length != sizeof(PacketHeader) + sizeof(HbPayload)) {
        return false;
    }

    HbPayload payload{};
    std::memcpy(&payload, data + sizeof(PacketHeader), sizeof(payload));
    sender = ntohl(payload.sender_id);
    return sender == header.sender_id;
}

} // namespace

FailureDetector::FailureDetector(NodeId local_id, const Endpoint& bind_ep, OnNodeFailed on_fail)
    : m_local_id(local_id), m_bind_ep(bind_ep), m_on_fail(std::move(on_fail)) {}

FailureDetector::~FailureDetector() {
    stop();
}

void FailureDetector::start() {
    if (m_running.load()) return;

    m_socket = Socket::udp_listener(m_bind_ep.port, true);
    m_running = true;
    try {
        m_thread = std::thread(&FailureDetector::run, this);
    } catch (...) {
        m_running = false;
        m_socket.close();
        throw;
    }
}

void FailureDetector::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    m_socket.close();
}

void FailureDetector::update_peers(const std::vector<NodeInfo>& peers) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(m_mtx);
    std::unordered_map<NodeId, PeerState> updated;
    for (const auto& peer : peers) {
        if (peer.id == m_local_id) continue;

        auto existing = m_peer_states.find(peer.id);
        updated.emplace(peer.id, existing == m_peer_states.end()
            ? PeerState{now, true}
            : existing->second);
    }
    m_peers = peers;
    m_peer_states = std::move(updated);
}

bool FailureDetector::is_alive(NodeId id) const {
    if (id == m_local_id) return true;
    std::lock_guard<std::mutex> lk(m_mtx);
    auto peer = m_peer_states.find(id);
    return peer != m_peer_states.end() && peer->second.alive;
}

void FailureDetector::run() {
    constexpr auto interval = std::chrono::milliseconds(kHbIntervalMs);
    constexpr auto timeout = interval * kHbMissLimit;
    auto next_heartbeat = std::chrono::steady_clock::now();
    uint8_t buffer[256];

    while (m_running) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_heartbeat) {
            std::vector<NodeInfo> peers;
            {
                std::lock_guard<std::mutex> lk(m_mtx);
                peers = m_peers;
            }

            const auto packet = make_heartbeat(m_local_id);
            for (const auto& peer : peers) {
                if (peer.id == m_local_id || peer.hb.port == 0) continue;

                sockaddr_in destination{};
                destination.sin_family = AF_INET;
                destination.sin_port = htons(peer.hb.port);
                if (inet_pton(AF_INET, peer.hb.ip.c_str(), &destination.sin_addr) != 1) {
                    std::cerr << "[failure detector " << m_local_id
                              << "] invalid heartbeat address for node " << peer.id
                              << ": " << peer.hb.ip << "\n";
                    continue;
                }
                if (sendto(m_socket.fd(), packet.data(), packet.size(), 0,
                           reinterpret_cast<sockaddr*>(&destination),
                           sizeof(destination)) < 0) {
                    std::cerr << "[failure detector " << m_local_id
                              << "] failed to send heartbeat to node " << peer.id << "\n";
                }
            }
            next_heartbeat = now + interval;
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(m_socket.fd(), &read_fds);
        timeval wait{0, 100000};
        const int ready = select(m_socket.fd() + 1, &read_fds, nullptr, nullptr, &wait);
        if (ready > 0 && FD_ISSET(m_socket.fd(), &read_fds)) {
            sockaddr_in source{};
            socklen_t source_length = sizeof(source);
            const ssize_t received = recvfrom(
                m_socket.fd(), buffer, sizeof(buffer), 0,
                reinterpret_cast<sockaddr*>(&source), &source_length);
            NodeId sender = 0;
            if (received > 0 &&
                parse_heartbeat(buffer, static_cast<size_t>(received), sender)) {
                std::lock_guard<std::mutex> lk(m_mtx);
                auto peer = m_peer_states.find(sender);
                if (peer != m_peer_states.end()) {
                    peer->second.last_seen = std::chrono::steady_clock::now();
                    peer->second.alive = true;
                }
            }
        }

        std::vector<NodeId> failed;
        const auto checked_at = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            for (auto& [id, state] : m_peer_states) {
                if (state.alive && checked_at - state.last_seen >= timeout) {
                    state.alive = false;
                    failed.push_back(id);
                }
            }
        }
        for (NodeId id : failed) {
            std::cerr << "[failure detector " << m_local_id
                      << "] missed " << kHbMissLimit
                      << " heartbeats from node " << id << "\n";
            if (m_on_fail) m_on_fail(id);
        }
    }
}

} // namespace ss
