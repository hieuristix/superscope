#pragma once
#include "network/types.h"
#include <functional>
#include <vector>

namespace ss {

// crash failure detector
struct FailureDetector {
    // invoked when a peer is suspected or confirmed dead
    using OnNodeFailed = std::function<void(NodeId)>;

    // initialize failure detector with local id, bind endpoint, and failure callback
    FailureDetector(NodeId local_id, const Endpoint& bind_ep, OnNodeFailed on_fail);

    // stop monitoring and release resources
    ~FailureDetector();

    // start background liveness monitoring
    void start();

    // stop background liveness monitoring
    void stop();

    // update the set of peers to monitor
    void update_peers(const std::vector<NodeInfo>& peers);

    // return true if the given node is currently considered alive
    bool is_alive(NodeId id) const;

private:
    // unique id of the local satellite
    NodeId m_local_id;

    // local endpoint for liveness traffic
    Endpoint m_bind_ep;

    // callback invoked when a peer fails
    OnNodeFailed m_on_fail;

    // current set of monitored peers
    std::vector<NodeInfo> m_peers;
};

} // namespace ss
