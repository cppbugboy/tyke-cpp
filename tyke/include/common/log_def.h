/**
 * @file log_def.h
 * @brief Tyke框架日志宏定义
 * @author Nick
 * @date 2026/04/16
 *
 * 提供四个日志宏：LOG_DEBUG、LOG_INFO、LOG_WARN、LOG_ERROR，
 * 基于spdlog实现，自动记录源文件名、行号和函数名。
 *
 * 使用前需确保TykeLog已初始化（由TykeFramework::Start自动完成）。
 * 日志级别由TykeLog::SetLogLevel控制，Release模式下DEBUG级别默认不输出。
 *
 * 使用示例：
 * @code
 *   LOG_INFO("server started on: {}", listen_uuid);
 *   LOG_ERROR("encode failed: {}", error_msg);
 * @endcode
 */

#ifndef TYKE_LOG_DEF_H
#define TYKE_LOG_DEF_H

#include <spdlog/spdlog.h>

/**
 * @brief 调试级别日志宏，用于输出详细的调试信息
 *
 * 仅在DEBUG级别启用时输出，适用于编解码细节、数据收发等调试场景。
 */
#define LOG_DEBUG(...)                                                                                                 \
spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug,   \
__VA_ARGS__)

/**
 * @brief 信息级别日志宏，用于输出关键业务节点信息
 *
 * 适用于框架启动/停止、连接建立/断开、请求分发等正常业务流程。
 */
#define LOG_INFO(...)                                                                                                  \
spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info,    \
__VA_ARGS__)

/**
 * @brief 警告级别日志宏，用于输出可恢复的异常情况
 *
 * 适用于路由未找到、过滤器中断链、重复发送响应等非致命异常。
 */
#define LOG_WARN(...)                                                                                                  \
spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn,    \
__VA_ARGS__)

/**
 * @brief 错误级别日志宏，用于输出不可恢复的错误
 *
 * 适用于编解码失败、IPC连接断开、加密失败、框架启动失败等严重错误。
 */
#define LOG_ERROR(...)                                                                                                 \
spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err,     \
__VA_ARGS__)

#endif //TYKE_LOG_DEF_H
