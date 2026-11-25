#pragma once
#include <xev/volk.h>
#include <xev/pipeline/pipeline.h>
#include <xev/renderer.h>
#include <xev/resource/buffer.h>
#include <xev/resource/image.h>
#include <xev/global_descriptor_set.h>
#include <xev/resource_manager.h>
#include <xev/pipeline_manager.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace viewer {

struct Satellite {
  float theta{0.0f};          // Initial angle / longitude (radians)
  float phi{0.0f};            // Inclination / latitude (radians)
  float radius{1.05f};        // Distance from Earth center (Earth radius = 1.0)
  float speed{0.5f};          // Angular speed (radians/sec)
  glm::vec3 color{0.0f};      // Color (unshaded; if {0,0,0}, assigned random in init)
  float sphereRadius{0.025f}; // Radius of the satellite sphere
  glm::vec3 pos{0.0f};        // Current 3D position (precomputed / updated)
  float _pad0{0.0f};
  glm::vec3 planeNormal{0.0f, 1.0f, 0.0f}; // Precomputed plane unit normal
  float _pad1{0.0f};
  glm::vec3 u{1.0f, 0.0f, 0.0f};           // Precomputed in-plane basis vector u
  float _pad2{0.0f};
  glm::vec3 v{0.0f, 0.0f, 1.0f};           // Precomputed in-plane basis vector v
  float _pad3{0.0f};
};

struct DrawInfo {
  glm::mat3 transform{1.0f};
  glm::vec4 uvBounds{0.0f, 0.0f, 1.0f, 1.0f};
  glm::vec3 color{1.0f};
  uint32_t texID{0xFFFFFFFF};
  uint32_t isMSDF{0};
};

struct PushConstants {
  VkDeviceAddress infoBuffer{0};
  VkDeviceAddress satelliteBuffer{0};
  glm::vec3 camPos{0.0f, 0.0f, 3.5f};
  float time{0.0f};
  glm::vec3 camForward{0.0f, 0.0f, -1.0f};
  uint32_t numSatellites{0};
  glm::vec3 camRight{1.0f, 0.0f, 0.0f};
  uint32_t skyTexId{0xFFFFFFFF};
  glm::vec3 camUp{0.0f, 1.0f, 0.0f};
  uint32_t earthTexId{0xFFFFFFFF};
  float fov{glm::radians(45.0f)};
  float aspect{1.0f};
  float earthRadius{1.0f};
  float skyAspect{1.0f};
};

struct Pipeline : public xev::RenderPipeline, public xev::Renderer {
  Pipeline(xev::PipelineManager& pipelineManager,
           xev::ResourceManager& resourceManager,
           uint32_t numFramesInFlight);
  ~Pipeline();

  void update_draw_infos(const std::vector<DrawInfo>& infos, uint32_t frameIdx);
  void update_satellites(const std::vector<Satellite>& satellites, uint32_t frameIdx);

  void draw(VkCommandBuffer cmd,
            const xev::Image& colorImage,
            const xev::GlobalDescriptorSet& descSet,
            PushConstants pc,
            uint32_t currFrameIdx);

  static constexpr uint32_t MAX_QUADS = 16;
  static constexpr uint32_t MAX_SATELLITES = 256;

 private:
  void begin_render(VkCommandBuffer cmd, const xev::Image& colorImage);

  xev::PipelineManager& m_pipelineManager;
  xev::ResourceManager& m_resourceManager;
  std::vector<xev::Buffer> m_drawInfoBuffers;
  std::vector<xev::Buffer> m_satelliteBuffers;
  uint32_t m_numQuads{0};
  uint32_t m_numSatellites{0};
};

} // namespace viewer

using Pipeline = viewer::Pipeline;
