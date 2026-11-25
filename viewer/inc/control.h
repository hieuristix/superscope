#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace viewer {

struct ArcballControl {
  glm::vec3 target{0.0f};
  float distance{3.5f};
  float yaw{0.0f};
  float pitch{0.2f};
  float fov{glm::radians(45.0f)};

  void rotate(float deltaX, float deltaY);
  void zoom(float delta);

  glm::vec3 get_position() const;
  glm::vec3 get_forward() const;
  glm::vec3 get_right() const;
  glm::vec3 get_up() const;
};

} // namespace viewer

using ArcballControl = viewer::ArcballControl;
