#pragma once
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

struct Image {
  uint32_t width{0};
  uint32_t height{0};
  std::vector<uint8_t> buffer{};
};

struct Sample {
  glm::vec2 position{0, 0};
  float sensitivity{0.0f};
};

struct Sensor {
  uint32_t width{128};
  uint32_t height{128};
  std::vector<Sample> samples{};
};

glm::vec2 shirley_chiu_mapping(const glm::vec2& uv);
void init_sensor(Sensor& sensor);
void expose(const glm::vec2& position,
            const Sensor& sensor,
            const Image& skybox,
            Image& image,
            float exposure_time);

