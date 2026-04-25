/**
 * @file log_def.h
 * @brief 日志系统宏定义。基于spdlog库封装日志输出宏，支持DEBUG/INFO/WARN/ERROR四个级别。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <spdlog/spdlog.h>

#define LOG_DEBUG(...)                                                                                                 \
spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug,   \
__VA_ARGS__)

#define LOG_INFO(...)                                                                                                  \
spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info,    \
__VA_ARGS__)

#define LOG_WARN(...)                                                                                                  \
spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn,    \
__VA_ARGS__)

#define LOG_ERROR(...)                                                                                                 \
spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err,     \
__VA_ARGS__)