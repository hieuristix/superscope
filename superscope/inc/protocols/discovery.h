#pragma once
#include "network/types.h"
#include "network/socket.h"
#include <functional>
#include <vector>

namespace ss {

// udp discovery protocol for satellite join, swarm views, and leader lookup
struct Discovery {
    // invoked when a satellite broadcasts a join packet
    using OnJoin        = std::function<void(const NodeInfo&, const Endpoint&)>;

    // invoked when a swarm view packet arrives
    using OnView        = std::function<void(const std::vector<NodeInfo>&)>;

    // invoked on a client when a leader announces its tcp endpoint
    using OnLeader      = std::function<void(NodeId, const Endpoint&)>;

    // invoked on servers when a client broadcasts a leader query
    using OnWhoIsLeader = std::function<void(const Endpoint&)>;

    // bind port 0 for clients (ephemeral unicast reply) or kDiscoveryPort for servers
    Discovery(NodeId id, const Endpoint& tcp, const Endpoint& hb, uint16_t listen_port);

    // announce this satellite to the constellation
    void broadcast_join();

    // send the current membership list unicast to one target
    void send_view(const Endpoint& target, const std::vector<NodeInfo>& members);

    // broadcast the current membership list to all satellites
    void broadcast_view(const std::vector<NodeInfo>& members);

    // broadcast a leader discovery query from a client
    void broadcast_who_is_leader();

    // reply unicast to a client with the leader's tcp endpoint
    void reply_leader(const Endpoint& target, const Endpoint& leader_tcp);

    // receive and dispatch one discovery packet (timeout_ms < 0 blocks indefinitely)
    bool poll_once(OnJoin on_join, OnView on_view, OnLeader on_leader,
                   OnWhoIsLeader on_who = nullptr, int timeout_ms = -1);

private:
    // unique id of this node or client
    NodeId m_id;

    // local tcp endpoint advertised in join packets
    Endpoint m_tcp;

    // local heartbeat udp endpoint advertised in join packets
    Endpoint m_hb;

    // udp socket used for multicast, broadcast, and unicast discovery
    Socket m_sock;
};

} // namespace ss
