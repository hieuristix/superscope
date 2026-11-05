cmake_minimum_required(VERSION 3.22)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

add_executable(test_capture_direct ${SOURCE_FILES})
target_include_directories(test_capture_direct PUBLIC inc ../ext/glm ../ext/stb ../ext/tinyexr)
target_link_libraries(test_capture_direct PUBLIC spdlog::spdlog)
target_compile_definitions(test_capture_direct PRIVATE SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_executable(test_capture_raycast ${SOURCE_FILES})
target_include_directories(test_capture_raycast PUBLIC inc ../ext/glm ../ext/stb ../ext/tinyexr)
target_link_libraries(test_capture_raycast PUBLIC spdlog::spdlog)
target_compile_definitions(test_capture_raycast PRIVATE SS_RAYCAST SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")
