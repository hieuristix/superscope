#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

struct Image;

struct Satellite {
  glm::vec3 pos; // precomputed
  glm::vec3 orbitNorm;
  float orbitHeight; // 0 is geodesic, 1 is a point
  float radius;
  float speed;
  float phase; // offset in time of the orbit
};

struct World {
  Image skyEquirect;
};
