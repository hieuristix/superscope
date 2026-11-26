#include "sensor.h"
#include <algorithm>
#include <cmath>

namespace ss {

static constexpr double kPi = 3.14159265358979323846;

Sensor::Sensor(const CameraFOV& fov) : m_fov(fov) {}

Tile Sensor::capture(NodeId id, SeqNum seq, const OrbitalCoords& orbit) {
    Tile t = {
        .id     = (uint32_t)id,
        .seq    = seq,
        .hops   = 0,
        .w      = m_fov.w,
        .h      = m_fov.h,
        .pixels = std::vector<float>(m_fov.w * m_fov.h),
    };

    uint32_t rng = (uint32_t)(id * 2654435761u + seq * 1234567891u);
    auto rand_f = [&]() -> float {
        rng = rng * 1664525u + 1013904223u;
        return (float)rng / (float)0xFFFFFFFFu;
    };

    double cx = m_fov.w * 0.5, cy = m_fov.h * 0.5;
    double max_dist = std::sqrt(cx*cx + cy*cy);
    double base = 0.5 + 0.4 * std::sin(orbit.anomaly_deg * kPi / 180.0);

    for (uint32_t y = 0; y < m_fov.h; ++y) {
        for (uint32_t x = 0; x < m_fov.w; ++x) {
            double dx = x - cx, dy = y - cy;
            double falloff = std::cos(std::sqrt(dx*dx+dy*dy) / max_dist * (kPi/2.0));
            float px = (float)(base * falloff) + (rand_f() - 0.5f) * 0.05f;
            t.pixels[y * m_fov.w + x] = std::max(0.f, std::min(1.f, px));
        }
    }
    return t;
}

} // namespace ss
