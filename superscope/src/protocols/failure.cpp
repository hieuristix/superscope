#include "protocols/failure.h"

namespace ss {

FailureDetector::FailureDetector(NodeId local_id, const Endpoint& bind_ep, OnNodeFailed on_fail)
    : m_local_id(local_id), m_bind_ep(bind_ep), m_on_fail(std::move(on_fail)) {}

FailureDetector::~FailureDetector() {
    stop();
}

void FailureDetector::start() {
    // TODO
}

void FailureDetector::stop() {
    // TODO
}

void FailureDetector::update_peers(const std::vector<NodeInfo>& peers) {
    m_peers = peers;
}

bool FailureDetector::is_alive(NodeId /*id*/) const {
    return true;
}

} // namespace ss
