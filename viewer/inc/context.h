#pragma once
#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>
#include <vector>

#include <xev/engine.h>
#include <xev/window.h>
#include <xev/resource/image.h>
#include "pipeline.h"
#include "control.h"

inline glm::mat3 screen_quad_transform(glm::vec2 topLeft,
                                       glm::vec2 bottomRight) {
  glm::vec2 size = bottomRight - topLeft;
  glm::vec2 pos = (topLeft + bottomRight) - 1.0f;
  return glm::mat3(glm::vec3(size.x, 0.0f, 0.0f),
                   glm::vec3(0.0f, size.y, 0.0f),
                   glm::vec3(pos.x, pos.y, 1.0f));
}

struct Context {
  Context();
  ~Context();

  void handle_inputs();
  void update(float dt);
  void draw();
  void run();

  std::unique_ptr<xev::Window> window;
  std::unique_ptr<xev::Engine> engine;
  std::unique_ptr<viewer::Pipeline> pipeline;

  viewer::ArcballControl orbitCam;
  std::vector<viewer::Satellite> satellites;
  std::vector<viewer::DrawInfo> quadDrawInfos;

  xev::Image skyTexture;
  xev::Image earthTexture;
  uint32_t skyTexId{0xFFFFFFFF};
  uint32_t earthTexId{0xFFFFFFFF};

  bool running{true};
  bool isDraggingOrbit{false};
  float lastMouseX{0.0f};
  float lastMouseY{0.0f};
  float simTime{0.0f};
  uint64_t lastTick{0};

 private:
  void load_textures();
  void init_satellites();
  void init_quads();
};
