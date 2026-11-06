#pragma once
#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>

#include <superscope/imaging.h>
#include <xev/engine.h>
#include <xev/frame_context.h>
#include <xev/global_descriptor_set.h>
#include <xev/hot_exec.h>
#include <xev/pipeline_manager.h>
#include <xev/renderer2D.h>
#include <xev/resource_manager.h>
#include <xev/window.h>

inline glm::mat3 screen_quad_transform(glm::vec2 topLeft,
                                       glm::vec2 bottomRight) {
  glm::vec2 size = bottomRight - topLeft;
  glm::vec2 pos = (topLeft + bottomRight) - 1.0f;
  return glm::mat3(glm::vec3(size.x, 0.0f, 0.0f),
                   glm::vec3(0.0f, size.y, 0.0f),
                   glm::vec3(pos.x, pos.y, 1.0f));
}

struct Context {
  std::unique_ptr<xev::Window> window;
  std::unique_ptr<xev::Engine> engine;
  std::unique_ptr<xev::Renderer2D> renderer2D;
  Image equirect;
  bool running{true};

  Context() {
    const std::filesystem::path root{SS_ROOT_DIR};
    load_image(root / "assets" / "starmap_2020_8k.exr", equirect);
    SS_INFO("Loaded equirect texture ({}x{})", equirect.width, equirect.height);

    window = std::make_unique<xev::Window>("Superscope Viewer", 800, 600);
    engine = std::make_unique<xev::Engine>(window->get_native());
    engine->init_file_system();
    engine->init_swapchain();
    engine->init_resource_manager();
    engine->init_hot_exec();
    engine->init_global_descriptor_set();
    engine->init_pipeline_manager();
    engine->init_frame_context();

    renderer2D = std::make_unique<xev::Renderer2D>(
        *engine->pipelineManager, *engine->resourceManager,
        engine->frameContext->get_num_frames());
  }

  void handle_inputs() {
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
    }
  }

  void draw() {
    VkCommandBuffer cmdbuf = engine->frameContext->acquire_frame();
    if (cmdbuf == VK_NULL_HANDLE) return;

    const xev::Image& output_color =
        engine->frameContext->get_current_render_target();

    renderer2D->draw_rect(
        screen_quad_transform({0.0f, 0.0f}, {0.5f, 0.5f}),
        {1.0f, 1.0f, 1.0f});
    renderer2D->draw_rect(
        screen_quad_transform({0.0f, 0.5f}, {0.5f, 1.0f}),
        {1.0f, 1.0f, 1.0f});
    renderer2D->draw_rect(
        screen_quad_transform({0.5f, 0.0f}, {1.0f, 1.0f}),
        {1.0f, 1.0f, 1.0f});

    renderer2D->draw(cmdbuf, output_color, *engine->globalDescriptorSet,
                     engine->frameContext->get_current_index(),
                     {0.1f, 0.1f, 0.1f, 1.0f}, true);

    engine->submit_and_show(cmdbuf, output_color);
  }

  void run() {
    while (running) {
      handle_inputs();
      draw();
    }
  }
};
