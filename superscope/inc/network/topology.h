#pragma once
#include "network/types.h"
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ss {

// abstract routing topology for swarm membership and neighbor lookups
struct ITopology {
    virtual ~ITopology() = default;

    // insert a member if not already present
    virtual void add_member(const NodeInfo& info) = 0;

    // remove a member by id
    virtual void remove_member(NodeId id) = 0;

    // replace the entire membership list
    virtual void set_members(const std::vector<NodeInfo>& members) = 0;

    // return the next hop tcp endpoint for election token forwarding
    virtual std::optional<Endpoint> next_hop() const = 0;

    // return the upstream tcp endpoint for tile aggregation forwarding
    virtual std::optional<Endpoint> upstream_hop() const = 0;

    // return the immediate neighbor nodes to monitor with heartbeats
    virtual std::vector<NodeInfo> peers() const = 0;

    // return all active member ids
    virtual std::vector<NodeId> all_ids() const = 0;

    // return full node info for all active members
    virtual std::vector<NodeInfo> all_members() const = 0;
};

// unidirectional logical ring ordered by ascending node id
struct RingTopology : ITopology {
    // create a ring view centered on local_id
    explicit RingTopology(NodeId local_id);

    // insert a member and keep the ring sorted by id
    void add_member(const NodeInfo& info) override;

    // remove a member from the ring
    void remove_member(NodeId id) override;

    // replace all members and sort the ring
    void set_members(const std::vector<NodeInfo>& members) override;

    // return the clockwise successor tcp endpoint
    std::optional<Endpoint> next_hop() const override;

    // return the upstream aggregation target (clockwise successor)
    std::optional<Endpoint> upstream_hop() const override;

    // return predecessor and successor node info
    std::vector<NodeInfo> peers() const override;

    // return sorted list of all member ids
    std::vector<NodeId> all_ids() const override;

    // return sorted list of all member node info
    std::vector<NodeInfo> all_members() const override;

private:
    // find the index of this node in m_order (-1 if absent)
    int local_index() const;

    // id of the local satellite
    NodeId m_local_id;

    // sorted ring order of active member ids
    std::vector<NodeId> m_order;

    // lookup table from node id to full endpoints
    std::unordered_map<NodeId, NodeInfo> m_info;

    // guards concurrent access from discovery, election, and bootstrap threads
    mutable std::mutex m_mtx;
};

} // namespace ss
