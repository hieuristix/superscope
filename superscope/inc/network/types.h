#pragma once
#include <cstdint>
#include <string>

namespace ss {

// unique identifier for a server or client
using NodeId    = uint32_t;

// monotonically increasing command sequence number from the leader
using SeqNum    = uint64_t;

// logical clock timestamp for synchronized capture
using LogicalTs = uint64_t;

// lifecycle state of a server node
enum class NodeState { Booting, Discovering, Electing, Worker, Leader };

// ip address and port pair
struct Endpoint {
    // ipv4 address string
    std::string ip;

    // tcp or udp port number
    uint16_t port{0};
};

// network identity of one satellite in the swarm
struct NodeInfo {
    // unique id of the satellite
    NodeId id{0};

    // tcp endpoint for election and tile traffic
    Endpoint tcp;

    // udp endpoint for heartbeat pulses
    Endpoint hb;
};

// orbital position of a satellite
struct OrbitalCoords {
    // altitude above earth in kilometers
    double altitude_km{0};

    // orbital inclination in degrees
    double inclination_deg{0};

    // position along the orbit in degrees
    double anomaly_deg{0};
};

// camera field of view and sensor resolution
struct CameraFOV {
    // horizontal field of view in degrees
    double h_deg{60};

    // vertical field of view in degrees
    double v_deg{45};

    // output tile width in pixels
    uint32_t w{320};

    // output tile height in pixels
    uint32_t h{240};
};

// default udp port for discovery (This is shared)
static constexpr uint16_t kDiscoveryPort   = 9000;

// base tcp port for lcr ring election
static constexpr uint16_t kRingTcpBase     = 10000;

// base udp port for heartbeats
static constexpr uint16_t kHbPortBase      = 11000;

// udp port for ordered capture multicast
static constexpr uint16_t kMulticastPort   = 12000;

// base tcp port for receiving aggregated tiles
static constexpr uint16_t kTilePortBase    = 13000;

// default tcp port for client connections to the leader (This is shared)
static constexpr uint16_t kClientPort      = 8000;

// interval between heartbeat pulses in milliseconds
static constexpr int kHbIntervalMs         = 500;

// consecutive missed heartbeats before declaring a node dead
static constexpr int kHbMissLimit          = 3;

// multicast group address for swarm control traffic
static constexpr const char* kMulticastGroup = "239.0.0.1";

// subnet broadcast address for discovery fallback
static constexpr const char* kBroadcastAddr  = "255.255.255.255";

} // namespace ss
