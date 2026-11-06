#include <chrono>
#include <filesystem>
#include <fstream>

#include "imaging.h"

int main() {
  const std::filesystem::path root{SS_ROOT_DIR};
  const std::filesystem::path out_dir = root / "captures";
  std::filesystem::create_directories(out_dir);

#ifdef SS_RAYCAST
  const std::string name = "raycast";
#else
  const std::string name = "direct";
#endif

  Image skybox;
  load_image(root / "assets" / "starmap_2020_8k.exr", skybox);

  Sensor sensor{256, 256};
  init_sensor(sensor);

  Image out_img;
  glm::vec2 center{0.5f, 0.5f};
  ImageRequest request{
      .topLeft = center - glm::vec2{0.25f, 0.5f},
      .bottomRight = center + glm::vec2{0.25f, 0.5f},
  };
  float exposure_time = 0.1f;

  auto t0 = std::chrono::high_resolution_clock::now();
  capture(center, sensor, skybox, out_img, exposure_time, request);
  auto t1 = std::chrono::high_resolution_clock::now();
  double elapsed_ms =
      std::chrono::duration<double, std::milli>(t1 - t0).count();

  save_image(out_dir / (name + ".png"), out_img);

  std::ofstream txt(out_dir / (name + ".txt"));
  txt << "mode: " << name << "\n"
      << "width: " << out_img.width << "\n"
      << "height: " << out_img.height << "\n"
      << "position: " << center.x << ", " << center.y << "\n"
      << "topLeft: " << request.topLeft.x << ", " << request.topLeft.y << "\n"
      << "bottomRight: " << request.bottomRight.x << ", "
      << request.bottomRight.y << "\n"
      << "exposure_time: " << exposure_time << "\n"
      << "elapsed_ms: " << elapsed_ms << "\n";

  SS_INFO("Saved {} capture ({:.2f} ms)", name, elapsed_ms);
  return 0;
}
