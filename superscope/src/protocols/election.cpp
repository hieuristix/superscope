#include "protocols/election.h"

namespace ss {

Election::Election(NodeId local_id, SendFn send_fn, OnLeaderElected on_elected)
    : m_local_id(local_id), m_send(std::move(send_fn)), m_on_elected(std::move(on_elected)) {}

void Election::start_election() {
    // TODO
}

void Election::on_message(NodeId /*from*/, const std::vector<uint8_t>& /*payload*/) {
    // TODO
}

void Election::on_membership_changed(const std::vector<NodeInfo>& members) {
    m_members = members;
}

bool Election::in_progress() const {
    return m_in_progress;
}

} // namespace ss
