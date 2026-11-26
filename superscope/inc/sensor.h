#pragma once
#include "network/types.h"
#include "protocols/aggregation.h"

namespace ss {

// simulated satellite photosensor with cosine falloff and noise
struct Sensor {
    // initialize sensor with camera field of view and resolution
    explicit Sensor(const CameraFOV& fov);

    // generate a synthetic sub-tile for the given satellite, epoch, and orbit
    Tile capture(NodeId id, SeqNum seq, const OrbitalCoords& orbit);

private:
    // camera geometry and output dimensions
    CameraFOV m_fov;
};

} // namespace ss
