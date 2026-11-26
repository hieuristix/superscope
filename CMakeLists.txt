cmake_minimum_required(VERSION 3.22)
project(superscope LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MAKE_APP_DEMO OFF CACHE BOOL "" FORCE)
set(MAKE_APP_CHOO OFF CACHE BOOL "" FORCE)
set(MAKE_APP_DEMO_ANIM OFF CACHE BOOL "" FORCE)
set(MAKE_TOOL_PAXER OFF CACHE BOOL "" FORCE)
set(MAKE_TOOL_BINDER OFF CACHE BOOL "" FORCE)
set(NB_USE_SUBMODULE_DEPS OFF CACHE BOOL "" FORCE)
set(XEV_ENABLE_NETWORK OFF CACHE BOOL "" FORCE)

option(SS_NET_ONLY "Build only superscope networking binaries (no viewer/xev)" OFF)

if(SS_NET_ONLY)
  add_subdirectory(ext/spdlog EXCLUDE_FROM_ALL)
  add_subdirectory(superscope)
else()
  add_subdirectory(ext/xev EXCLUDE_FROM_ALL)
  add_subdirectory(superscope)
  add_subdirectory(viewer)
endif()
