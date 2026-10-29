cmake_minimum_required(VERSION 3.22)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

add_executable(superscope ${SOURCE_FILES})
target_include_directories(superscope PUBLIC inc)
