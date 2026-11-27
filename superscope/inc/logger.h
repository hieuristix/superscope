#pragma once

#include <cstdlib>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

inline void ss_init_logger() {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>(spdlog::color_mode::always);
    auto logger = std::make_shared<spdlog::logger>("ss", sink);
    logger->set_pattern("[%H:%M:%S] %v");
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);
}

#define SS_INFO(...) spdlog::info(__VA_ARGS__)
#define SS_STR_(x) #x
#define SS_STR(x) SS_STR_(x)
#define SS_ERROR(fmt, ...) \
  spdlog::error("[" __FILE__ ":" SS_STR(__LINE__) "] " fmt, ##__VA_ARGS__)
#define SS_WARN(...) spdlog::warn(__VA_ARGS__)
#define SS_DEBUG(...) spdlog::debug(__VA_ARGS__)

#define SS_SERVER_INFO(id, fmt, ...) \
  spdlog::info("sat-{} \033[32mINFO\033[0m " fmt, id, ##__VA_ARGS__)
#define SS_SERVER_WARN(id, fmt, ...) \
  spdlog::warn("sat-{} \033[33mWARN\033[0m " fmt, id, ##__VA_ARGS__)
#define SS_SERVER_ERROR(id, fmt, ...) \
  spdlog::error("sat-{} \033[31mERROR\033[0m " fmt, id, ##__VA_ARGS__)

#define SS_ASSERT(expr, ...)                                            \
  do {                                                                  \
    if (!(expr)) {                                                      \
      spdlog::critical("[" __FILE__ ":" SS_STR(__LINE__) "] "           \
                       "Assertion failed: " #expr);                     \
      spdlog::critical("" __VA_ARGS__);                                 \
      std::abort();                                                     \
    }                                                                   \
  } while (0)
