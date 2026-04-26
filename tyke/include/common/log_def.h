/**
 * @file log_def.h
 * @brief 日志系统宏定义。基于spdlog库封装日志输出宏，支持DEBUG/INFO/WARN/ERROR四个级别。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <spdlog/spdlog.h>

#define LOG_DEBUG(...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::debug))            \
        {                                                                                                              \
            spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
                                              spdlog::level::debug, __VA_ARGS__);                                      \
        }                                                                                                              \
    } while (0)

#define LOG_INFO(...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::info))             \
        {                                                                                                              \
            spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
                                              spdlog::level::info, __VA_ARGS__);                                       \
        }                                                                                                              \
    } while (0)

#define LOG_WARN(...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::warn))             \
        {                                                                                                              \
            spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
                                              spdlog::level::warn, __VA_ARGS__);                                       \
        }                                                                                                              \
    } while (0)

#define LOG_ERROR(...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::err))              \
        {                                                                                                              \
            spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
                                              spdlog::level::err, __VA_ARGS__);                                        \
        }                                                                                                              \
    } while (0)
