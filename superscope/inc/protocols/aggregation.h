#pragma once
#include "network/types.h"
#include <cstddef>
#include <functional>
#include <vector>

namespace ss {

// grayscale sub-image tile produced by sensors and combined by aggregation
struct Tile {
    // id of the originating satellite
    uint32_t id{0};

    // capture epoch sequence number this tile belongs to
    uint64_t seq{0};

    // number of satellites already merged into this tile
    uint32_t hops{0};

    // tile width in pixels
    uint32_t w{0};

    // tile height in pixels
    uint32_t h{0};

    // row-major normalized pixel intensities in [0, 1]
    std::vector<float> pixels;
};

// serialize a tile into a wire-format byte buffer
std::vector<uint8_t> encode_tile(const Tile& t);

// deserialize a tile from a wire-format byte buffer
bool decode_tile(const uint8_t* data, size_t len, Tile& out);

// tile aggregation across the swarm
struct Aggregator {
    // send a tile to another node's tcp endpoint
    using SendTileFn    = std::function<void(const Endpoint&, const Tile&)>;

    // invoked when a final aggregated result is ready locally
    using OnResultReady = std::function<void(const Tile&)>;

    // initialize aggregator with local id and transport/result callbacks
    Aggregator(NodeId local_id, SendTileFn send_fn, OnResultReady on_ready);

    // submit a tile captured by this node's local sensor
    void submit_local(const Tile& tile);

    // submit a tile received over the network from another satellite
    void on_remote_tile(NodeId from, const Tile& tile);

    // update routing info when swarm membership or leader changes
    void update_topology(const std::vector<NodeInfo>& members, NodeId leader_id);

private:
    // unique id of the local satellite
    NodeId m_local_id;

    // callback used to transmit partial or final tiles to a target peer
    SendTileFn m_send;

    // callback invoked when final aggregation completes on this node
    OnResultReady m_on_ready;

    // latest known swarm members
    std::vector<NodeInfo> m_members;

    // current leader node id
    NodeId m_leader_id{0};
};

} // namespace ss
