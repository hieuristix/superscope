cmake_minimum_required(VERSION 3.22)

option(SS_RAYCAST "Enable raycast exposure" ON)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include")
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/include/superscope")
  file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/inc" "${CMAKE_CURRENT_BINARY_DIR}/include/superscope" SYMBOLIC)
endif()

set(LIB_SOURCES
  src/imaging.cpp
  src/binding.cpp
  src/impl.cpp
)

add_library(superscope_core STATIC ${LIB_SOURCES})
target_include_directories(superscope_core PUBLIC
  inc
  "${CMAKE_CURRENT_BINARY_DIR}/include"
  ../ext/glm
  ../ext/stb
  ../ext/tinyexr
)
target_link_libraries(superscope_core PUBLIC spdlog::spdlog)
target_compile_definitions(superscope_core PUBLIC SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")
if(SS_RAYCAST)
  target_compile_definitions(superscope_core PUBLIC SS_RAYCAST)
endif()

add_executable(superscope src/main.cpp)
target_link_libraries(superscope PRIVATE superscope_core)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

add_executable(test_capture_direct ${SOURCE_FILES})
target_include_directories(test_capture_direct PUBLIC inc ../ext/glm ../ext/stb ../ext/tinyexr)
target_link_libraries(test_capture_direct PUBLIC spdlog::spdlog)
target_compile_definitions(test_capture_direct PRIVATE SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_executable(test_capture_raycast ${SOURCE_FILES})
target_include_directories(test_capture_raycast PUBLIC inc ../ext/glm ../ext/stb ../ext/tinyexr)
target_link_libraries(test_capture_raycast PUBLIC spdlog::spdlog)
target_compile_definitions(test_capture_raycast PRIVATE SS_RAYCAST SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")
