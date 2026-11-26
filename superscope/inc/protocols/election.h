#pragma once
#include "network/types.h"
#include <cstdint>
#include <functional>
#include <vector>

namespace ss {

// leader election protocol
struct Election {
    // send an opaque protocol message to a peer's tcp endpoint
    using SendFn          = std::function<void(const Endpoint&, const std::vector<uint8_t>&)>;

    // notify the local node once a leader has been elected
    using OnLeaderElected = std::function<void(NodeId)>;

    // initialize election protocol with local id and transport callbacks
    Election(NodeId local_id, SendFn send_fn, OnLeaderElected on_elected);

    // start a new leader election round
    void start_election();

    // handle an incoming protocol message from another node
    void on_message(NodeId from, const std::vector<uint8_t>& payload);

    // update the protocol with the latest active swarm membership
    void on_membership_changed(const std::vector<NodeInfo>& members);

    // return true while an election round is currently running
    bool in_progress() const;

private:
    // unique id of the local satellite
    NodeId m_local_id;

    // callback to transmit election messages to any peer
    SendFn m_send;

    // callback invoked when a leader is chosen
    OnLeaderElected m_on_elected;

    // latest known swarm members
    std::vector<NodeInfo> m_members;

    // true while an election is underway
    bool m_in_progress{false};
};

} // namespace ss
