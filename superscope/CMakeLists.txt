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
  src/network/messages.cpp
  src/network/framing.cpp
  src/network/socket.cpp
  src/network/topology.cpp
  src/sensor.cpp
  src/stitcher.cpp
  src/server.cpp
  src/client.cpp
  src/protocols/discovery.cpp
  src/protocols/failure.cpp
  src/protocols/election.cpp
  src/protocols/sequencer.cpp
  src/protocols/aggregation.cpp
)

add_library(superscope_core STATIC ${LIB_SOURCES})
target_include_directories(superscope_core PUBLIC
  inc
  "${CMAKE_CURRENT_BINARY_DIR}/include"
  ../ext/glm
  ../ext/stb
  ../ext/tinyexr
)
find_package(Threads REQUIRED)
target_link_libraries(superscope_core PUBLIC spdlog::spdlog Threads::Threads)
target_compile_definitions(superscope_core PUBLIC SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")
if(SS_RAYCAST)
  target_compile_definitions(superscope_core PUBLIC SS_RAYCAST)
endif()

add_executable(superscope src/main.cpp)
target_link_libraries(superscope PRIVATE superscope_core)

add_executable(server src/server_main.cpp)
target_link_libraries(server PRIVATE superscope_core)

add_executable(client src/client_main.cpp src/client/repl.cpp)
target_link_libraries(client PRIVATE superscope_core)

add_executable(launcher src/launcher.cpp)
target_compile_definitions(launcher PRIVATE SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_executable(test_capture_direct src/main.cpp ${LIB_SOURCES})
target_include_directories(test_capture_direct PUBLIC inc ../ext/glm ../ext/stb ../ext/tinyexr)
target_link_libraries(test_capture_direct PUBLIC spdlog::spdlog Threads::Threads)
target_compile_definitions(test_capture_direct PRIVATE SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_executable(test_capture_raycast src/main.cpp ${LIB_SOURCES})
target_include_directories(test_capture_raycast PUBLIC inc ../ext/glm ../ext/stb ../ext/tinyexr)
target_link_libraries(test_capture_raycast PUBLIC spdlog::spdlog Threads::Threads)
target_compile_definitions(test_capture_raycast PRIVATE SS_RAYCAST SS_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_executable(test_election tests/election.cpp)
target_link_libraries(test_election PRIVATE superscope_core)

add_executable(test_failure tests/failure.cpp)
target_link_libraries(test_failure PRIVATE superscope_core)
