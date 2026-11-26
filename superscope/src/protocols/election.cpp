#include "protocols/election.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iterator>
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
    : m_local_id(local_id), m_send(std::move(send_fn)), m_on_elected(std::move(on_elected)) {}

void Election::start_election() {
    std::optional<Endpoint> destination;
    bool elect_self = false;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_in_progress) return;

        m_in_progress = true;
        m_participating = true;

        auto local = std::lower_bound(
            m_members.begin(), m_members.end(), m_local_id,
            [](const NodeInfo& member, NodeId id) { return member.id < id; });
        if (m_members.empty()) {
            m_in_progress = false;
            m_participating = false;
            elect_self = true;
        } else if (local == m_members.end() || local->id != m_local_id) {
            m_in_progress = false;
            m_participating = false;
        } else if (m_members.size() == 1) {
            m_in_progress = false;
            m_participating = false;
            elect_self = true;
        } else {
            auto next = std::next(local);
            if (next == m_members.end()) next = m_members.begin();
            destination = next->tcp;
        }
    }

    if (elect_self) {
        if (m_on_elected) m_on_elected(m_local_id);
    } else if (destination) {
        m_send(*destination, make_message(ElectionMessageType::Candidate, m_local_id));
    }
}

void Election::on_message(NodeId from, const std::vector<uint8_t>& payload) {
    ElectionMessageType type{};
    NodeId message_id = 0;
    if (!parse_message(payload, type, message_id)) return;

    std::optional<Endpoint> destination;
    std::optional<NodeId> elected;
    ElectionMessageType forward_type = type;
    NodeId forward_id = message_id;

    {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_members.size() <= 1) return;

        auto local = std::lower_bound(
            m_members.begin(), m_members.end(), m_local_id,
            [](const NodeInfo& member, NodeId id) { return member.id < id; });
        if (local == m_members.end() || local->id != m_local_id) return;
        auto sender = std::lower_bound(
            m_members.begin(), m_members.end(), from,
            [](const NodeInfo& member, NodeId id) { return member.id < id; });
        if (sender == m_members.end() || sender->id != from) return;
        auto candidate = std::lower_bound(
            m_members.begin(), m_members.end(), message_id,
            [](const NodeInfo& member, NodeId id) { return member.id < id; });
        if (candidate == m_members.end() || candidate->id != message_id) return;

        auto next = std::next(local);
        if (next == m_members.end()) next = m_members.begin();

        if (type == ElectionMessageType::Candidate) {
            if (message_id == m_local_id) {
                elected = m_local_id;
                forward_type = ElectionMessageType::Leader;
                forward_id = m_local_id;
                m_participating = false;
                destination = next->tcp;
            } else if (message_id > m_local_id) {
                m_participating = true;
                destination = next->tcp;
            } else if (!m_participating) {
                m_participating = true;
                forward_id = m_local_id;
                destination = next->tcp;
            }
        } else {
            if (message_id == m_local_id) {
                m_in_progress = false;
                m_participating = false;
                return;
            }
            m_in_progress = false;
            m_participating = false;
            elected = message_id;
            destination = next->tcp;
        }
    }

    if (elected && m_on_elected) m_on_elected(*elected);
    if (destination) m_send(*destination, make_message(forward_type, forward_id));
}

void Election::on_membership_changed(const std::vector<NodeInfo>& members) {
    bool restart = false;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        std::vector<NodeInfo> updated = members;
        std::sort(updated.begin(), updated.end(),
                  [](const NodeInfo& a, const NodeInfo& b) { return a.id < b.id; });
        updated.erase(std::unique(updated.begin(), updated.end(),
                                  [](const NodeInfo& a, const NodeInfo& b) {
                                      return a.id == b.id;
                                  }),
                      updated.end());

        const bool changed = updated.size() != m_members.size() ||
            !std::equal(updated.begin(), updated.end(), m_members.begin(),
                        [](const NodeInfo& a, const NodeInfo& b) {
                            return a.id == b.id;
                        });
        m_members = std::move(updated);
        if (changed && m_in_progress) {
            m_in_progress = false;
            m_participating = false;
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
