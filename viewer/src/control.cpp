#include "control.h"

namespace viewer {

void ArcballControl::rotate(float deltaX, float deltaY) {
  yaw -= deltaX * 0.005f;
  pitch += deltaY * 0.005f;
  pitch = glm::clamp(pitch, -1.55f, 1.55f);
}

void ArcballControl::zoom(float delta) {
  distance = glm::clamp(distance - delta * 0.25f, 1.2f, 25.0f);
}

glm::vec3 ArcballControl::get_position() const {
  float cy = std::cos(yaw);
  float sy = std::sin(yaw);
  float cp = std::cos(pitch);
  float sp = std::sin(pitch);
  return target + distance * glm::vec3(cp * sy, sp, cp * cy);
}

glm::vec3 ArcballControl::get_forward() const {
  return glm::normalize(target - get_position());
}

glm::vec3 ArcballControl::get_right() const {
  return glm::normalize(glm::cross(get_forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 ArcballControl::get_up() const {
  return glm::cross(get_right(), get_forward());
}

} // namespace viewer
