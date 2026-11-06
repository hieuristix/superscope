cmake_minimum_required(VERSION 3.22)
project(viewer)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

add_executable(viewer ${SOURCE_FILES})
target_include_directories(viewer PRIVATE inc)
target_link_libraries(viewer PRIVATE xev_core superscope_core)

set(VIEWER_BIN_DIR "${CMAKE_BINARY_DIR}/viewer/bin")
set(VIEWER_SHADER_DIR "${CMAKE_BINARY_DIR}/viewer/shaders")

set_target_properties(viewer PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${VIEWER_BIN_DIR}"
)

if(SLANGC_EXECUTABLE)
  file(MAKE_DIRECTORY "${VIEWER_SHADER_DIR}")
  set(RASTER_SLANG "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/raster.slang")
  set(RASTER_SPV "${VIEWER_SHADER_DIR}/raster.spv")
  add_custom_command(
    OUTPUT "${RASTER_SPV}"
    COMMAND ${SLANGC_EXECUTABLE}
      -I "${CMAKE_SOURCE_DIR}/ext/xev/xev/src/shaders"
      "${RASTER_SLANG}"
      -target spirv
      -o "${RASTER_SPV}"
    DEPENDS "${RASTER_SLANG}"
    COMMENT "Compiling viewer raster.slang to SPIR-V"
    VERBATIM
  )
  add_custom_target(viewer_shaders DEPENDS "${RASTER_SPV}")
  add_dependencies(viewer viewer_shaders)
endif()
