#pragma once
#include "network/types.h"
#include "protocols/discovery.h"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ss {

// leader identity returned by discovery
struct LeaderInfo {
    // node id of the active leader
    NodeId id{0};

    // tcp endpoint of the active leader
    Endpoint ep;
};

// swarm membership snapshot returned by the leader
struct SwarmInfo {
    // total number of active satellites
    uint32_t size{0};

    // ids of all active satellites
    std::vector<NodeId> nodes;
};

// job queue status returned by the leader
struct QueueInfo {
    // 0 if not queued, 1 if active, 2+ if waiting
    uint32_t position{0};

    // total number of clients currently queued
    uint32_t total{0};
};

// network client that discovers the leader and sends tcp requests
struct Client {
    // initialize client with a unique id and ephemeral udp discovery socket
    explicit Client(NodeId client_id);

    // return this client's unique id
    NodeId id() const { return m_id; }

    // return the currently cached leader, if any
    std::optional<LeaderInfo> cached_leader() const;

    // broadcast a leader query and update the cached leader
    std::optional<LeaderInfo> discover_leader();

    // query the leader over tcp for swarm size and active node ids
    std::optional<SwarmInfo> query_swarm();

    // query the leader over tcp for this client's job queue position
    std::optional<QueueInfo> query_queue();

    // submit a panorama capture job and return the raw ppm bytes
    std::optional<std::vector<uint8_t>> request_capture();

private:
    // discover the leader if not already cached; return false if none found
    bool ensure_leader();

    // clear cached leader info after a tcp connection failure
    void clear_leader();

    // unique identifier for this client instance
    NodeId m_id;

    // udp discovery client bound to an ephemeral port for unicast replies
    Discovery m_disc;

    // cached id and endpoint of the active leader
    std::optional<LeaderInfo> m_leader;
};

} // namespace ss
