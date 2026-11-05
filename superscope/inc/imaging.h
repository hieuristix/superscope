#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

#include <stb_image.h>
#include <stb_image_write.h>
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#include <tinyexr.h>
#include <glm/glm.hpp>
#include "logger.h"

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

struct ImageRequest {
  glm::vec2 topLeft;
  glm::vec2 bottomRight;
};

// this function maps grid point to uniform placement
// on the hemisphere, basically simulating fish-eye lens
glm::vec2 shirley_chiu_mapping(const glm::vec2& uv);

// initialize the the sensor. Automatically setting the
// sample points as if they're the location of light entering
// the fish eye
void init_sensor(Sensor& sensor);

glm::vec3 sample_image(const Image& image, const glm::vec2& uv);

void direct_pixel_expose(glm::vec3& outColor,
                         const glm::vec2& position,
                         const Sample& sample,
                         const Image& skybox,
                         float exposureTime);

void raycast_pixel_expose(glm::vec3& outColor,
                          const glm::vec2& position,
                          const Sample& sample,
                          const Image& skybox,
                          float exposureTime);

// perform ray-casting for given the sensor location the output
// image that accumulate according to the exposure time
void expose(const glm::vec2& position,
            const Sensor& sensor,
            const Image& skybox,
            Image& image,
            float exposure_time);

void capture(const glm::vec2& position,
             const Sensor& sensor,
             const Image& skybox,
             Image& image,
             float exposure_time,
             ImageRequest& request);

void load_image(const std::filesystem::path& path, Image& image);

void save_image(const std::filesystem::path& path, const Image& image);

#ifdef SS_FEATURE_VULKAN
// implement vulkan based ray casting here
#endif
