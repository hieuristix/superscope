#include "network/messages.h"
#include <arpa/inet.h>
#include <cstring>

namespace ss {

std::vector<uint8_t> encode_header(const PacketHeader& h) {
    PacketHeader w = h;
    w.sender_id   = htonl(h.sender_id);
    w.payload_len = htonl(h.payload_len);
    std::vector<uint8_t> out(sizeof(w));
    std::memcpy(out.data(), &w, sizeof(w));
    return out;
}

bool decode_header(const uint8_t* buf, size_t len, PacketHeader& out) {
    if (len < sizeof(PacketHeader)) return false;
    std::memcpy(&out, buf, sizeof(PacketHeader));
    if (out.magic[0] != 'S' || out.magic[1] != 'S') return false;
    out.sender_id   = ntohl(out.sender_id);
    out.payload_len = ntohl(out.payload_len);
    return true;
}

} // namespace ss
