#pragma once
#include "network/types.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ss {

// wire message types shared across tcp and udp
enum class MsgType : uint8_t {
    DiscoveryJoin  = 1,
    SwarmView      = 2,
    WhoIsLeader    = 3,
    LeaderAnnounce = 4,
    Heartbeat      = 5,
    ElectionMsg    = 6,
    CaptureCmd     = 8,
    RetransmitReq  = 9,
    TileChunk      = 10,
    JobRequest     = 11,
    JobResult      = 12,
    QueueQuery     = 13,
    QueueStatus    = 14,
    SwarmQuery     = 15,
    SwarmStatus    = 16,
};

// fixed 12-byte header prepended to every packet
struct PacketHeader {
    // magic bytes {'S', 'S'} for validation
    uint8_t magic[2];

    // identifies the payload type
    MsgType type;

    // alignment padding
    uint8_t _pad;

    // id of the sending node
    uint32_t sender_id;

    // byte length of the following payload
    uint32_t payload_len;
};
static_assert(sizeof(PacketHeader) == 12);

// payload for discovery join and leader announce packets
struct JoinPayload {
    // id of the announcing node
    uint32_t node_id;

    // tcp ipv4 address string
    char tcp_ip[16];

    // tcp port in network byte order
    uint16_t tcp_port;

    // heartbeat ipv4 address string
    char hb_ip[16];

    // heartbeat port in network byte order
    uint16_t hb_port;
};

// one member record inside a swarm view packet
struct SwarmEntry {
    // id of the member node
    uint32_t node_id;

    // tcp ipv4 address string
    char tcp_ip[16];

    // tcp port in network byte order
    uint16_t tcp_port;

    // heartbeat ipv4 address string
    char hb_ip[16];

    // heartbeat port in network byte order
    uint16_t hb_port;
};

// payload for a udp heartbeat packet
struct HbPayload {
    // id of the alive sender
    uint32_t sender_id;
};

// payload for a sequenced multicast capture command
struct CapturePayload {
    // leader-assigned sequence number
    uint64_t seq_num;

    // leader-assigned logical timestamp
    uint64_t logical_ts;
};

// header preceding raw float pixels in a tile chunk
struct TileHeader {
    // originating satellite tile id
    uint32_t tile_id;

    // capture epoch sequence number
    uint64_t seq_num;

    // number of satellites merged into this tile
    uint32_t stage_hops;

    // tile width in pixels
    uint32_t width;

    // tile height in pixels
    uint32_t height;

    // number of float pixels following this header
    uint32_t pixel_count;
};

// response payload for a client queue position query
struct QueueStatusPayload {
    // 0 if not queued, 1 if active, 2+ if waiting
    uint32_t position;

    // total number of clients currently in the queue
    uint32_t total;
};

// response header for a client swarm size query (followed by `size` node ids)
struct SwarmStatusPayload {
    // number of active servers in the swarm
    uint32_t size;
};

// serialize a packet header into network byte order
std::vector<uint8_t> encode_header(const PacketHeader& h);

// parse a packet header from raw bytes and validate magic bytes
bool decode_header(const uint8_t* buf, size_t len, PacketHeader& out);

} // namespace ss
