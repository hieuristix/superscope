#include "protocols/election.h"
#include <arpa/inet.h>
#include <cstring>
#include <optional>

namespace ss {

namespace {

enum class ElectionMessageType : uint8_t {
    Candidate = 1,
    Leader = 2,
};

std::vector<uint8_t> make_message(ElectionMessageType type, NodeId id) {
    const uint32_t network_id = htonl(id);
    std::vector<uint8_t> payload(1 + sizeof(network_id));
    payload[0] = static_cast<uint8_t>(type);
    std::memcpy(payload.data() + 1, &network_id, sizeof(network_id));
    return payload;
}

bool parse_message(const std::vector<uint8_t>& payload,
                   ElectionMessageType& type, NodeId& id) {
    if (payload.size() != 1 + sizeof(uint32_t)) return false;
    if (payload[0] != static_cast<uint8_t>(ElectionMessageType::Candidate) &&
        payload[0] != static_cast<uint8_t>(ElectionMessageType::Leader)) {
        return false;
    }

    uint32_t network_id = 0;
    std::memcpy(&network_id, payload.data() + 1, sizeof(network_id));
    type = static_cast<ElectionMessageType>(payload[0]);
    id = ntohl(network_id);
    return true;
}

} // namespace

Election::Election(NodeId local_id, SendFn send_fn, OnLeaderElected on_elected)
    : m_local_id(local_id)
    , m_send(std::move(send_fn))
    , m_on_elected(std::move(on_elected))
    , m_ring(local_id) {}

void Election::reset() {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_in_progress = false;
}

void Election::start_election() {
    std::optional<Endpoint> destination;
    bool elect_self = false;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_in_progress) return;

        auto ids = m_ring.all_ids();
        if (ids.empty() || (ids.size() == 1 && ids[0] == m_local_id)) {
            elect_self = true;
        } else if (auto next = m_ring.next_hop()) {
            m_in_progress = true;
            destination = *next;
        }
    }

    if (elect_self) {
        if (m_on_elected) m_on_elected(m_local_id);
    } else if (destination && m_send) {
        m_send(*destination, make_message(ElectionMessageType::Candidate, m_local_id));
    }
}

void Election::on_message_lcr(const std::vector<uint8_t>& payload) {
    ElectionMessageType type{};
    NodeId message_id = 0;
    if (!parse_message(payload, type, message_id)) return;

    std::optional<Endpoint> destination;
    std::optional<NodeId> elected;
    ElectionMessageType forward_type = type;
    NodeId forward_id = message_id;

    {
        std::lock_guard<std::mutex> lk(m_mtx);
        auto next = m_ring.next_hop();
        if (!next) return;

        if (type == ElectionMessageType::Candidate) {
            if (message_id == m_local_id) {
                elected = m_local_id;
                forward_type = ElectionMessageType::Leader;
                forward_id = m_local_id;
                destination = *next;
            } else if (message_id > m_local_id) {
                m_in_progress = true;
                destination = *next;
            } else if (!m_in_progress) {
                m_in_progress = true;
                forward_id = m_local_id;
                destination = *next;
            }
        } else {
            m_in_progress = false;
            if (message_id == m_local_id) return;
            elected = message_id;
            destination = *next;
        }
    }

    if (elected && m_on_elected) m_on_elected(*elected);
    if (destination && m_send) m_send(*destination, make_message(forward_type, forward_id));
}

void Election::on_membership_changed(const std::vector<NodeInfo>& members) {
    bool restart = false;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        auto before = m_ring.all_ids();
        m_ring.set_members(members);
        if (m_ring.all_ids() != before && m_in_progress) {
            m_in_progress = false;
            restart = true;
        }
    }
    if (restart) start_election();
}

bool Election::in_progress() const {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_in_progress;
}

} // namespace ss
