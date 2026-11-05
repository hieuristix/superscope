cmake_minimum_required(VERSION 3.22)
project(superscope LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(ext/spdlog EXCLUDE_FROM_ALL)
add_subdirectory(superscope)
