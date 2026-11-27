#pragma once
#include "network/types.h"
#include "network/messages.h"
#include "network/socket.h"
#include "sensor.h"
#include "protocols/discovery.h"
#include "protocols/failure.h"
#include "protocols/election.h"
#include "protocols/sequencer.h"
#include "protocols/aggregation.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace ss {

// active leader state: handles client sessions, job queue, and capture sequencing
struct LeaderCtx {
    // bind client tcp server and multicast sender for the elected leader
    LeaderCtx(NodeId id, uint16_t client_port,
              const std::string& mcast_group, uint16_t mcast_port);

    // stop accepting clients and close sockets
    ~LeaderCtx();

    // start the client tcp accept loop thread
    void start();

    // shut down client listener and wake any queued jobs
    void stop();

    // update active swarm member ids for client swarm queries
    void update_members(const std::vector<NodeId>& ids);

    // stamp and multicast a new capture command to all workers
    void issue_capture();

    // store a completed tile received from aggregation
    void on_final_tile(const Tile& t);

private:
    // accept incoming client tcp connections and spawn a handler per client
    void accept_loop();

    // serve one client request (queue query, swarm query, or capture job)
    void handle_client(int fd);

    // compute a client's 1-based position and total length in the job queue
    QueueStatusPayload queue_status(NodeId client_id);

    // unique id of the leader satellite
    NodeId m_id;

    // multicast group ip for capture commands
    std::string m_mcast_group;

    // multicast udp port for capture commands
    uint16_t m_mcast_port;

    // latest snapshot of active member ids for swarm size queries
    std::vector<NodeId> m_member_ids;

    // guards concurrent access to m_member_ids
    std::mutex m_members_mtx;

    // sequence number and logical clock generator
    Sequencer m_seq;

    // listening tcp socket for incoming client connections
    Socket m_srv_sock;

    // udp socket for sending multicast capture commands
    Socket m_mcast_sock;

    // completed panorama tiles keyed by capture sequence number
    std::map<SeqNum, Tile> m_tiles;

    // guards concurrent access to m_tiles
    std::mutex m_tiles_mtx;

    // fifo queue of client ids waiting for capture jobs
    std::deque<NodeId> m_job_queue;

    // guards concurrent access to m_job_queue
    std::mutex m_queue_mtx;

    // wakes waiting client threads when the front of m_job_queue changes
    std::condition_variable m_queue_cv;

    // flag controlling the accept loop and active handlers
    std::atomic<bool> m_running{false};

    // background thread running accept_loop
    std::thread m_accept_thr;
};

// active worker state: receives ordered captures, shoots tiles, and delegates aggregation
struct WorkerCtx {
    // initialize worker multicast listener, tile receiver, sensor, and aggregator
    WorkerCtx(NodeId id, const CameraFOV& fov, const OrbitalCoords& orbit,
              const std::string& mcast_group, uint16_t mcast_port,
              uint16_t tile_recv_port);

    // stop background threads and close sockets
    ~WorkerCtx();

    // launch multicast and tile receiver threads
    void start();

    // stop and join multicast and tile receiver threads
    void stop();

    // forward updated swarm membership and leader id to the aggregator
    void update_topology(const std::vector<NodeInfo>& members, NodeId leader_id);

private:
    // listen for multicast capture commands and feed them into m_hbq
    void mcast_loop();

    // accept incoming tcp tile connections and feed them into m_agg
    void tile_recv_loop();

    // trigger local sensor capture when m_hbq delivers a command in order
    void on_capture(SeqNum seq, const std::vector<uint8_t>& payload);

    // send a tile over tcp to the target peer endpoint chosen by m_agg
    void send_tile(const Endpoint& dst, const Tile& t);

    // a unique id given by the launcher
    NodeId m_id;

    // the physical location of the satellite
    OrbitalCoords m_orbit;

    // multicast group ip for capture commands
    std::string m_mcast_group;

    // multicast udp port for capture commands
    uint16_t m_mcast_port;

    // local tcp port listening for incoming tiles
    uint16_t m_tile_recv_port;

    // simulated camera photosensor
    Sensor m_sensor;

    // hold-back queue enforcing total order on capture commands
    HoldBackQueue m_hbq;

    // tile aggregator
    Aggregator m_agg;

    // udp socket joined to the capture multicast group
    Socket m_mcast_sock;

    // listening tcp socket for incoming tiles
    Socket m_tile_srv;

    // flag controlling worker background loops
    std::atomic<bool> m_running{false};

    // background threads for multicast reception and tile reception
    std::thread m_mcast_thr, m_tile_thr;
};

// top-level satellite node coordinating discovery, election, failure detection, and roles
struct ServerNode {
    // initialize node identity, discovery, failure detector, and election
    ServerNode(NodeId id, const Endpoint& tcp, const Endpoint& hb,
               const CameraFOV& fov, const OrbitalCoords& orbit,
               uint16_t disc_port);

    // stop all subsystems and join threads
    ~ServerNode();

    // bind protocol listeners, broadcast join, and start runtime loops
    void start();

    // shut down all threads, roles, and sockets
    void stop();

    // return the current lifecycle state of this node
    NodeState state() const { return m_state.load(); }

private:
    // this changes the state of the node
    void transition(NodeState s);

    // promote this node to leader and start LeaderCtx
    void become_leader();

    // assign this node as a worker and start WorkerCtx
    void become_worker(NodeId leader_id);

    // handle a newly joined satellite and broadcast the updated swarm view
    void on_joined(const NodeInfo& info, const Endpoint& reply_ep);

    // merge an incoming swarm view and notify protocols of membership changes
    void on_view(const std::vector<NodeInfo>& members);

    // remove a failed node from topology and trigger re-election
    void on_fail(NodeId dead);

    // switch into leader or worker role once an election finishes
    void on_elected(NodeId leader);

    // insert a member into m_members; return true if newly added
    bool add_member(const NodeInfo& info);

    // remove a member from m_members by id
    void remove_member(NodeId id);

    // return sorted snapshot of all known members
    std::vector<NodeInfo> all_members() const;

    // return sorted snapshot of all known member ids
    std::vector<NodeId> all_ids() const;

    // notify election, failure detector, and worker of current membership
    void sync_protocols();

    // periodically retry discovery join or broadcast leader view
    void bootstrap_loop();

    // continuously poll udp discovery packets
    void disc_loop();

    // accept incoming tcp election protocol messages
    void elec_loop();

    // send an opaque framed election message to a peer endpoint
    void send_election_msg(const Endpoint& dst, const std::vector<uint8_t>& payload);

    // a unique id given by the launcher
    NodeId m_id;

    // local tcp and heartbeat endpoints
    Endpoint m_tcp, m_hb;

    // camera field of view parameters
    CameraFOV m_fov;

    // the physical location of the satellite
    OrbitalCoords m_orbit;

    // current lifecycle state of this node
    std::atomic<NodeState> m_state{NodeState::Booting};

    // id of the currently elected leader (0 if unknown)
    std::atomic<NodeId> m_leader_id{0};

    // active swarm membership table keyed by node id
    std::map<NodeId, NodeInfo> m_members;

    // guards concurrent access to m_members
    mutable std::mutex m_members_mtx;

    // udp discovery protocol handler
    std::unique_ptr<Discovery> m_disc;

    // failure detector
    std::unique_ptr<FailureDetector> m_fd;

    // leader election protocol
    std::unique_ptr<Election> m_elec;

    // leader role context (active only when state is Leader)
    std::unique_ptr<LeaderCtx> m_leader;

    // worker role context (active only when state is Worker)
    std::unique_ptr<WorkerCtx> m_worker;

    // listening tcp socket for incoming election protocol messages
    Socket m_elec_srv;

    // flag controlling node background loops
    std::atomic<bool> m_running{false};

    // background threads for discovery, election, and bootstrap
    std::thread m_disc_thr, m_elec_thr, m_boot_thr;
};

} // namespace ss
