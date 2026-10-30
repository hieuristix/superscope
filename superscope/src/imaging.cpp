#include "imaging.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

glm::vec2 shirley_chiu_mapping(const glm::vec2& uv) {
  float a = 2.0f * uv.x - 1.0f;
  float b = 2.0f * uv.y - 1.0f;
  if (a == 0.0f && b == 0.0f) return {0.0f, 0.0f};

  float r, phi;
  if (a * a > b * b) {
    r = a;
    phi = (std::numbers::pi_v<float> / 4.0f) * (b / a);
  } else {
    r = b;
    phi = (std::numbers::pi_v<float> / 2.0f) -
          (std::numbers::pi_v<float> / 4.0f) * (a / b);
  }
  return {r * std::cos(phi), r * std::sin(phi)};
}

void init_sensor(Sensor& sensor) {
  sensor.samples.resize(sensor.width * sensor.height);
  for (uint32_t i = 0; i < sensor.height; ++i) {
    for (uint32_t j = 0; j < sensor.width; ++j) {
      glm::vec2 uv{(j + 0.5f) / sensor.width, (i + 0.5f) / sensor.height};
      glm::vec2 disk = shirley_chiu_mapping(uv);
      float r2 = glm::dot(disk, disk);
      float cos_theta = std::sqrt(std::max(0.0f, 1.0f - r2));
      sensor.samples[i * sensor.width + j] = {disk, cos_theta};
    }
  }
}

void expose(const glm::vec2& position,
            const Sensor& sensor,
            const Image& skybox,
            Image& image,
            float exposure_time) {
  image.width = sensor.width;
  image.height = sensor.height;
  image.buffer.resize(image.width * image.height);

  std::mt19937 rng{std::random_device{}()};

  for (uint32_t i = 0; i < sensor.height; ++i) {
    for (uint32_t j = 0; j < sensor.width; ++j) {
      const auto& sample = sensor.samples[i * sensor.width + j];

      // 1. parallel transport the sensor location to position
      glm::vec2 sample_uv = glm::fract(position + sample.position);

      // 2. sample image buffer
      uint32_t sx = std::min(
          static_cast<uint32_t>(sample_uv.x * skybox.width), skybox.width - 1);
      uint32_t sy =
          std::min(static_cast<uint32_t>(sample_uv.y * skybox.height),
                   skybox.height - 1);
      float radiance = skybox.buffer[sy * skybox.width + sx];

      // 3. add noise depend on sensor sensitivity
      float signal = radiance * sample.sensitivity * exposure_time;
      std::normal_distribution<float> noise(0.0f,
                                            1.0f - sample.sensitivity + 0.01f);
      float val = std::clamp(signal + noise(rng), 0.0f, 255.0f);

      // 4. store the final sample on image
      image.buffer[i * image.width + j] = static_cast<uint8_t>(val);
    }
  }
}

