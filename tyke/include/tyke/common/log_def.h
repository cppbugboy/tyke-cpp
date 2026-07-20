/**
 * @file log_def.h
 * @brief 日志系统宏定义。基于spdlog封装DEBUG/INFO/WARN/ERROR四级日志输出宏，
 *        自动携带源文件、行号和函数名信息。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 每个宏在执行日志输出前先检查日志级别是否启用（should_log），
 * 避免不必要的参数求值和格式化开销。
 *
 * @note 使用前需确保已通过spdlog::set_default_logger()设置了默认logger，
 *       否则default_logger_raw()返回nullptr，宏不会产生输出。
 */

#pragma once

#include <spdlog/spdlog.h>

/**
 * @brief 输出DEBUG级别日志
 * @note 仅在spdlog调试级别启用时产生输出
 */
#define LOG_DEBUG(...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::debug))            \
        {                                                                                                              \
            spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
                                              spdlog::level::debug, __VA_ARGS__);                                      \
        }                                                                                                              \
    } while (0)

/**
 * @brief 输出INFO级别日志
 */
#define LOG_INFO(...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::info))             \
        {                                                                                                              \
            spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
                                              spdlog::level::info, __VA_ARGS__);                                       \
        }                                                                                                              \
    } while (0)

/**
 * @brief 输出WARN级别日志
 */
#define LOG_WARN(...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::warn))             \
        {                                                                                                              \
            spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
                                              spdlog::level::warn, __VA_ARGS__);                                       \
        }                                                                                                              \
    } while (0)

/**
 * @brief 输出ERROR级别日志
 */
#define LOG_ERROR(...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::err))              \
        {                                                                                                              \
            spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
                                              spdlog::level::err, __VA_ARGS__);                                        \
        }                                                                                                              \
    } while (0)
