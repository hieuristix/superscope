#include "stitcher.h"
#include <algorithm>
#include <cstdio>

namespace ss {

Tile Stitcher::average(const Tile& a, const Tile& b) {
    Tile r = {
        .id     = a.id,
        .seq    = a.seq,
        .hops   = a.hops + 1,
        .w      = a.w,
        .h      = a.h,
        .pixels = std::vector<float>(a.pixels.size()),
    };
    for (size_t i = 0; i < a.pixels.size(); ++i)
        r.pixels[i] = (a.pixels[i] + b.pixels[i]) * 0.5f;
    return r;
}

bool Stitcher::save_ppm(const std::string& path, const Tile& t) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", t.w, t.h);
    for (float p : t.pixels) {
        uint8_t v = (uint8_t)(std::max(0.f, std::min(1.f, p)) * 255.f);
        uint8_t rgb[3] = {v, v, v};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

std::vector<uint8_t> Stitcher::to_buffer(const Tile& t) {
    char hdr[64];
    int hlen = std::snprintf(hdr, sizeof(hdr), "P6\n%u %u\n255\n", t.w, t.h);
    std::vector<uint8_t> out;
    out.reserve((size_t)hlen + t.pixels.size() * 3);
    out.insert(out.end(), (uint8_t*)hdr, (uint8_t*)hdr + hlen);
    for (float p : t.pixels) {
        uint8_t v = (uint8_t)(std::max(0.f, std::min(1.f, p)) * 255.f);
        out.push_back(v); out.push_back(v); out.push_back(v);
    }
    return out;
}

} // namespace ss
