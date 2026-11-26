#include "protocols/aggregation.h"
#include "network/messages.h"
#include <arpa/inet.h>
#include <cstring>

namespace ss {

std::vector<uint8_t> encode_tile(const Tile& t) {
    TileHeader h = {
        .tile_id     = htonl(t.id),
        .seq_num     = 0,
        .stage_hops  = htonl(t.hops),
        .width       = htonl(t.w),
        .height      = htonl(t.h),
        .pixel_count = htonl((uint32_t)t.pixels.size()),
    };
    uint32_t seq_hi = htonl((uint32_t)(t.seq >> 32));
    uint32_t seq_lo = htonl((uint32_t)(t.seq & 0xFFFFFFFF));
    std::memcpy(&h.seq_num, &seq_hi, 4);
    std::memcpy((uint8_t*)&h.seq_num + 4, &seq_lo, 4);

    std::vector<uint8_t> out(sizeof(h) + t.pixels.size() * sizeof(float));
    std::memcpy(out.data(), &h, sizeof(h));
    std::memcpy(out.data() + sizeof(h), t.pixels.data(), t.pixels.size() * sizeof(float));
    return out;
}

bool decode_tile(const uint8_t* data, size_t len, Tile& out) {
    if (len < sizeof(TileHeader)) return false;
    TileHeader h{}; std::memcpy(&h, data, sizeof(h));
    out.id   = ntohl(h.tile_id);
    out.hops = ntohl(h.stage_hops);
    out.w    = ntohl(h.width);
    out.h    = ntohl(h.height);
    uint32_t pc = ntohl(h.pixel_count);
    uint32_t hi, lo;
    std::memcpy(&hi, &h.seq_num, 4);
    std::memcpy(&lo, (uint8_t*)&h.seq_num + 4, 4);
    out.seq = ((uint64_t)ntohl(hi) << 32) | ntohl(lo);
    if (len < sizeof(h) + pc * sizeof(float)) return false;
    out.pixels.resize(pc);
    std::memcpy(out.pixels.data(), data + sizeof(h), pc * sizeof(float));
    return true;
}

Aggregator::Aggregator(NodeId local_id, SendTileFn send_fn, OnResultReady on_ready)
    : m_local_id(local_id), m_send(std::move(send_fn)), m_on_ready(std::move(on_ready)) {}

void Aggregator::submit_local(const Tile& /*tile*/) {
    // TODO
}

void Aggregator::on_remote_tile(NodeId /*from*/, const Tile& /*tile*/) {
    // TODO
}

void Aggregator::update_topology(const std::vector<NodeInfo>& members, NodeId leader_id) {
    m_members   = members;
    m_leader_id = leader_id;
}

} // namespace ss
