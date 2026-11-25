#include "context.h"
#include <stb_image.h>
#include <random>
#include <xev/frame_context.h>
#include <xev/global_descriptor_set.h>
#include <xev/hot_exec.h>
#include <xev/logger.h>
#include <xev/pipeline_manager.h>
#include <xev/resource_manager.h>
#include <xev/window.h>
#include <iostream>

static bool load_texture_file(const std::filesystem::path& path,
                              xev::Image& img,
                              xev::ResourceManager& manager,
                              xev::HotExec& hotExec,
                              xev::GlobalDescriptorSet& descSet,
                              uint32_t& outTexId) {
  int w = 0, h = 0, c = 0;
  stbi_uc* data = stbi_load(path.string().c_str(), &w, &h, &c, 4);
  if (!data) {
    XEV_ERROR("Failed to load texture file: {}", path.string());
    return false;
  }

  img.width = static_cast<uint32_t>(w);
  img.height = static_cast<uint32_t>(h);
  img.format = VK_FORMAT_R8G8B8A8_UNORM;
  img.flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  img.host_data.assign(data, data + (w * h * 4));
  stbi_image_free(data);

  manager.alloc(img);
  img.upload(manager, hotExec);
  outTexId = descSet.set(img);
  XEV_INFO("Loaded texture {} ({}x{}) bound as texID {}", path.filename().string(), w, h, outTexId);
  return true;
}

Context::Context() {
  window = std::make_unique<xev::Window>("Superscope Viewer", 1280, 720);
  engine = std::make_unique<xev::Engine>(window->get_native());
  engine->init_file_system();
  engine->init_swapchain();
  engine->init_resource_manager();
  engine->init_hot_exec();
  engine->init_global_descriptor_set();
  engine->init_pipeline_manager();
  engine->init_frame_context();

  pipeline = std::make_unique<viewer::Pipeline>(
      *engine->pipelineManager, *engine->resourceManager,
      engine->frameContext->get_num_frames());

  load_textures();
  init_satellites();
  init_quads();

  lastTick = SDL_GetTicks();
}

Context::~Context() {
  if (earthTexture.on_device()) {
    engine->resourceManager->free(earthTexture);
  }
  if (skyTexture.on_device()) {
    engine->resourceManager->free(skyTexture);
  }
}

void Context::load_textures() {
  const std::filesystem::path root{SS_ROOT_DIR};

  load_texture_file(root / "assets" / "sky_tonemap_texture.jpg", skyTexture,
                    *engine->resourceManager, *engine->hotExec,
                    *engine->globalDescriptorSet, skyTexId);

  load_texture_file(root / "assets" / "earth_bluemarble.jpg", earthTexture,
                    *engine->resourceManager, *engine->hotExec,
                    *engine->globalDescriptorSet, earthTexId);
}

void Context::init_satellites() {
  satellites.clear();
  // Satellites orbiting Earth at different angles, radii, and speeds
  // Color is unshaded and defaults to random if not specified
  satellites.push_back({.theta = 0.0f, .phi = 0.00f, .radius = 1.05f, .speed = 0.8f});
  satellites.push_back({.theta = 1.5f, .phi = 0.35f, .radius = 1.05f, .speed = 0.6f});
  satellites.push_back({.theta = 3.0f, .phi = 0.45f, .radius = 1.05f, .speed = 0.95f});
  satellites.push_back({.theta = 4.2f, .phi = 0.55f, .radius = 1.05f, .speed = 0.5f});
  satellites.push_back({.theta = 6.7f, .phi = 0.65f, .radius = 1.05f, .speed = 0.7f});

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(0.15f, 1.0f);
  for (auto& sat : satellites) {
    if (glm::length(sat.color) < 0.001f) {
      sat.color = glm::vec3(dist(rng), dist(rng), dist(rng));
    }
    // Precompute orbital plane slice basis vectors: u, v, planeNormal, and initial position
    float cosPhi = std::cos(sat.phi);
    float sinPhi = std::sin(sat.phi);
    float cosTheta = std::cos(sat.theta);
    float sinTheta = std::sin(sat.theta);
    sat.u = glm::vec3(cosPhi * sinTheta, sinPhi, cosPhi * cosTheta);
    sat.planeNormal = glm::normalize(glm::vec3(-sinPhi * sinTheta, cosPhi, -sinPhi * cosTheta));
    sat.v = glm::cross(sat.planeNormal, sat.u);
    sat.pos = sat.radius * sat.u;
  }
}

void Context::init_quads() {
  quadDrawInfos = {
      // Top-left: Capture (instID 0)
      viewer::DrawInfo{
          .transform = screen_quad_transform({0.0f, 0.0f}, {0.5f, 0.5f}),
          .uvBounds = {0.0f, 0.0f, 1.0f, 1.0f},
          .color = {1.0f, 1.0f, 1.0f},
      },
      // Bot-left: Sky (instID 1)
      viewer::DrawInfo{
          .transform = screen_quad_transform({0.0f, 0.5f}, {0.5f, 1.0f}),
          .uvBounds = {0.0f, 0.0f, 1.0f, 1.0f},
          .color = {1.0f, 1.0f, 1.0f},
      },
      // Right: Orbit (instID 2)
      viewer::DrawInfo{
          .transform = screen_quad_transform({0.5f, 0.0f}, {1.0f, 1.0f}),
          .uvBounds = {0.0f, 0.0f, 1.0f, 1.0f},
          .color = {1.0f, 1.0f, 1.0f},
      },
  };
}

void Context::handle_inputs() {
  int winW = 0, winH = 0;
  SDL_GetWindowSize(window->get_native(), &winW, &winH);
  float rightThreshold = static_cast<float>(winW) * 0.5f;
  float bottomThreshold = static_cast<float>(winH) * 0.5f;

  auto is_camera_panel = [&](float x, float y) {
    // Active on Right panel OR Bottom-Left (Sky) panel
    return (x >= rightThreshold) || (x < rightThreshold && y >= bottomThreshold);
  };

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT) {
      running = false;
    }
    if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && window &&
        e.window.windowID == SDL_GetWindowID(window->get_native())) {
      running = false;
    }
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
      running = false;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
      if (is_camera_panel(e.button.x, e.button.y)) {
        isDraggingOrbit = true;
        lastMouseX = e.button.x;
        lastMouseY = e.button.y;
      }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
      isDraggingOrbit = false;
    }
    if (e.type == SDL_EVENT_MOUSE_MOTION) {
      if (isDraggingOrbit) {
        orbitCam.rotate(e.motion.xrel, e.motion.yrel);
      }
    }
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
      float mx = 0.0f, my = 0.0f;
      SDL_GetMouseState(&mx, &my);
      if (is_camera_panel(mx, my)) {
        orbitCam.zoom(e.wheel.y);
      }
    }
  }
}

void Context::update(float dt) {
  simTime += dt;
  for (auto& sat : satellites) {
    sat.theta += sat.speed * dt;
    sat.pos = sat.radius * (std::cos(sat.speed * simTime) * sat.u + std::sin(sat.speed * simTime) * sat.v);
  }
}

void Context::draw() {
  VkCommandBuffer cmdbuf = engine->frameContext->acquire_frame();
  if (cmdbuf == VK_NULL_HANDLE) return;

  const xev::Image& outputColor = engine->frameContext->get_current_render_target();
  uint32_t currIdx = engine->frameContext->get_current_index();

  pipeline->update_draw_infos(quadDrawInfos, currIdx);
  pipeline->update_satellites(satellites, currIdx);

  int winW = 0, winH = 0;
  SDL_GetWindowSize(window->get_native(), &winW, &winH);
  float rightAspect = (static_cast<float>(winW) * 0.5f) / static_cast<float>(winH > 0 ? winH : 1);
  float skyAspect = static_cast<float>(winW) / static_cast<float>(winH > 0 ? winH : 1);

  viewer::PushConstants pc = {
      .camPos = orbitCam.get_position(),
      .time = simTime,
      .camForward = orbitCam.get_forward(),
      .numSatellites = static_cast<uint32_t>(satellites.size()),
      .camRight = orbitCam.get_right(),
      .skyTexId = skyTexId,
      .camUp = orbitCam.get_up(),
      .earthTexId = earthTexId,
      .fov = orbitCam.fov,
      .aspect = rightAspect,
      .earthRadius = 1.0f,
      .skyAspect = skyAspect,
  };

  pipeline->draw(cmdbuf, outputColor, *engine->globalDescriptorSet, pc, currIdx);
  engine->submit_and_show(cmdbuf, outputColor);
}

void Context::run() {
  while (running) {
    uint64_t now = SDL_GetTicks();
    float dt = static_cast<float>(now - lastTick) / 1000.0f;
    lastTick = now;

    handle_inputs();
    update(dt);
    draw();
  }
}
