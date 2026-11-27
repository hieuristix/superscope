#pragma once
#include "network/topology.h"
#include "network/types.h"
#include <cstdint>
#include <functional>
#include <mutex>
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

    // clear in-progress state so a stalled election can be restarted
    void reset();

    // handle an incoming lcr election message
    void on_message_lcr(const std::vector<uint8_t>& payload);

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

    // ring overlay used to route election tokens clockwise
    RingTopology m_ring;

    // guards ring and election state shared by the server threads
    mutable std::mutex m_mtx;

    // true while an election is underway
    bool m_in_progress{false};
};

} // namespace ss
