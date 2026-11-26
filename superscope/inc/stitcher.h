#pragma once
#include "protocols/aggregation.h"
#include <string>
#include <vector>

namespace ss {

// utility functions for blending tiles and encoding ppm output images
struct Stitcher {
    // pixel-wise average of two matching tiles
    static Tile average(const Tile& a, const Tile& b);

    // write a tile to disk as a binary ppm (p6) image
    static bool save_ppm(const std::string& path, const Tile& t);

    // encode a tile as a binary ppm (p6) byte buffer for tcp transfer
    static std::vector<uint8_t> to_buffer(const Tile& t);
};

} // namespace ss
