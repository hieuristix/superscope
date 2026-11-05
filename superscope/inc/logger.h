#pragma once

#include <cstdlib>
#include <spdlog/spdlog.h>

#define SS_INFO(...) spdlog::info(__VA_ARGS__)
#define SS_STR_(x) #x
#define SS_STR(x) SS_STR_(x)
#define SS_ERROR(fmt, ...) \
  spdlog::error("[" __FILE__ ":" SS_STR(__LINE__) "] " fmt, ##__VA_ARGS__)
#define SS_WARN(...) spdlog::warn(__VA_ARGS__)
#define SS_DEBUG(...) spdlog::debug(__VA_ARGS__)

#define SS_ASSERT(expr, ...)                                            \
  do {                                                                  \
    if (!(expr)) {                                                      \
      spdlog::critical("[" __FILE__ ":" SS_STR(__LINE__) "] "           \
                       "Assertion failed: " #expr);                     \
      spdlog::critical("" __VA_ARGS__);                                 \
      std::abort();                                                     \
    }                                                                   \
  } while (0)
