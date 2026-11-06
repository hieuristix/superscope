#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_IMPLEMENTATION
#include "imaging.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

// this is for mapping grid to uniform point
// on hemisphere, basically simulating fish-eye lens
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

// This fill
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

glm::vec3 sample_image(const Image& image, const glm::vec2& uv) {
  glm::vec2 wrapped = glm::fract(uv);
  uint32_t sx =
      std::min(static_cast<uint32_t>(wrapped.x * image.width), image.width - 1);
  uint32_t sy = std::min(static_cast<uint32_t>(wrapped.y * image.height),
                         image.height - 1);
  size_t idx = (sy * image.width + sx) * 3;
  return {image.buffer[idx], image.buffer[idx + 1], image.buffer[idx + 2]};
}

void direct_pixel_expose(glm::vec3& outColor,
                         const glm::vec2& position,
                         const Sample& sample,
                         const Image& skybox,
                         const float exposureTime) {
  static thread_local std::mt19937 rng{std::random_device{}()};

  // 1. parallel transport the sensor hemisphere location to position
  // A hemisphere covers [-0.25, 0.25] in longitude and [-0.5, 0.5] in latitude
  glm::vec2 sample_uv =
      position + sample.position * glm::vec2{0.25f, 0.5f};

  // 2. sample image buffer
  glm::vec3 radiance = sample_image(skybox, sample_uv);

  // 3. add noise depend on sensor sensitivity
  glm::vec3 signal = radiance * sample.sensitivity * exposureTime;
  std::normal_distribution<float> noise(
      0.0f, (1.0f - sample.sensitivity) * 0.005f);
  outColor = glm::max(
      signal + glm::vec3{noise(rng), noise(rng), noise(rng)}, 0.0f);
}

void raycast_pixel_expose(glm::vec3& outColor,
                          const glm::vec2& position,
                          const Sample& sample,
                          const Image& skybox,
                          float exposureTime) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> dist(-0.002f, 0.002f);
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);

  // Project pixel's hemisphere point onto the flat aperture disk (height = sqrt(1 - r^2))
  float aperture_height =
      std::sqrt(std::max(0.0f, 1.0f - glm::dot(sample.position, sample.position)));

  uint32_t num_rays = std::max(1u, static_cast<uint32_t>(exposureTime * 16.0f));
  glm::vec3 accum{0.0f};

  for (uint32_t r = 0; r < num_rays; ++r) {
    // Ray only enters if it passes through the foreshortened aperture opening
    if (unit(rng) > aperture_height) continue;

    glm::vec2 disk = sample.position + glm::vec2{dist(rng), dist(rng)};
    if (glm::dot(disk, disk) > 1.0f) continue;

    glm::vec2 sample_uv = position + disk * glm::vec2{0.25f, 0.5f};
    accum += sample_image(skybox, sample_uv);
  }

  outColor = (accum / static_cast<float>(num_rays)) * exposureTime;
}

void expose(const glm::vec2& position,
            const Sensor& sensor,
            const Image& skybox,
            Image& image,
            const float exposureTime) {
  image.width = sensor.width;
  image.height = sensor.height;
  image.buffer.resize(image.width * image.height * 3);

  for (uint32_t i = 0; i < sensor.height; ++i) {
    for (uint32_t j = 0; j < sensor.width; ++j) {
      const auto& sample = sensor.samples[i * sensor.width + j];
      glm::vec3 val{0.0f};
#ifdef SS_RAYCAST
      raycast_pixel_expose(val, position, sample, skybox, exposureTime);
#else
      direct_pixel_expose(val, position, sample, skybox, exposureTime);
#endif

      size_t idx = (i * image.width + j) * 3;
      image.buffer[idx] = val.r;
      image.buffer[idx + 1] = val.g;
      image.buffer[idx + 2] = val.b;
    }
  }
}

void capture(const glm::vec2& position,
             const Sensor& sensor,
             const Image& skybox,
             Image& image,
             float exposure_time,
             ImageRequest& request) {
  Image exposed;
  expose(position, sensor, skybox, exposed, exposure_time);

  // Sensor hemisphere covers [position - {0.25, 0.5}, position + {0.25, 0.5}]
  glm::vec2 sensor_tl = position - glm::vec2{0.25f, 0.5f};
  glm::vec2 sensor_br = position + glm::vec2{0.25f, 0.5f};
  glm::vec2 span = sensor_br - sensor_tl;

  glm::vec2 uv0 = glm::clamp((request.topLeft - sensor_tl) / span, 0.0f, 1.0f);
  glm::vec2 uv1 =
      glm::clamp((request.bottomRight - sensor_tl) / span, 0.0f, 1.0f);

  uint32_t x0 = static_cast<uint32_t>(uv0.x * exposed.width);
  uint32_t y0 = static_cast<uint32_t>(uv0.y * exposed.height);
  uint32_t x1 = std::max(x0 + 1, static_cast<uint32_t>(uv1.x * exposed.width));
  uint32_t y1 = std::max(y0 + 1, static_cast<uint32_t>(uv1.y * exposed.height));

  image.width = x1 - x0;
  image.height = y1 - y0;
  image.buffer.resize(image.width * image.height * 3);

  for (uint32_t y = 0; y < image.height; ++y) {
    for (uint32_t x = 0; x < image.width; ++x) {
      size_t src_idx = ((y0 + y) * exposed.width + (x0 + x)) * 3;
      size_t dst_idx = (y * image.width + x) * 3;
      image.buffer[dst_idx] = exposed.buffer[src_idx];
      image.buffer[dst_idx + 1] = exposed.buffer[src_idx + 1];
      image.buffer[dst_idx + 2] = exposed.buffer[src_idx + 2];
    }
  }
}

void load_image(const std::filesystem::path& path, Image& image) {
  int w, h, c;
  if (path.extension() == ".exr") {
    float* rgba = nullptr;
    const char* err = nullptr;
    int ret = LoadEXR(&rgba, &w, &h, path.string().c_str(), &err);
    SS_ASSERT(ret == TINYEXR_SUCCESS, "Failed to load EXR {}: {}",
              path.string(), err ? err : "");
    image.width = static_cast<uint32_t>(w);
    image.height = static_cast<uint32_t>(h);
    image.buffer.resize(static_cast<size_t>(w) * h * 3);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
      image.buffer[i * 3 + 0] = rgba[i * 4 + 0];
      image.buffer[i * 3 + 1] = rgba[i * 4 + 1];
      image.buffer[i * 3 + 2] = rgba[i * 4 + 2];
    }
    free(rgba);
    return;
  }
  uint8_t* data = stbi_load(path.string().c_str(), &w, &h, &c, 3);
  SS_ASSERT(data != nullptr, "Failed to load image: {}", path.string());
  image.width = static_cast<uint32_t>(w);
  image.height = static_cast<uint32_t>(h);
  image.buffer.resize(static_cast<size_t>(w) * h * 3);
  for (size_t i = 0; i < image.buffer.size(); ++i) {
    image.buffer[i] = data[i] / 255.0f;
  }
  stbi_image_free(data);
}

void tone_mapping(const Image& image, std::vector<uint8_t>& out) {
  out.resize(image.buffer.size());

  // Auto-exposure based on mean luminance
  double sum = 0.0;
  for (float v : image.buffer) sum += std::max(0.0f, v);
  float mean = static_cast<float>(sum / std::max<size_t>(1, image.buffer.size()));
  float gain = (mean > 1e-6f) ? (0.05f / mean) : 1.0f;

  for (size_t i = 0; i < image.buffer.size(); ++i) {
    float x = std::max(0.0f, image.buffer[i]) * gain;
    float mapped = x / (1.0f + x);  // Reinhard tone mapping
    float gamma = std::pow(mapped, 1.0f / 2.2f);
    out[i] = static_cast<uint8_t>(std::clamp(gamma * 255.0f, 0.0f, 255.0f));
  }
}

void save_image(const std::filesystem::path& path, const Image& image) {
  std::vector<uint8_t> out;
  tone_mapping(image, out);
  int ok = stbi_write_png(path.string().c_str(), image.width, image.height, 3,
                          out.data(), image.width * 3);
  SS_ASSERT(ok != 0, "Failed to save image: {}", path.string());
}

#ifdef SS_FEATURE_VULKAN
// implement vulkan based ray casting here
#endif
