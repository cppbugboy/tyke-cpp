/**
 * @file tyke_log.h
 * @brief Tyke日志管理器
 * @author Nick
 * @date 2026/04/17
 *
 * TykeLog负责初始化和管理spdlog日志系统。
 * 提供日志文件轮转、日志级别设置等功能。
 */

#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <spdlog/logger.h>

#include "common/tyke_result.h"
#include "component/singleton.hpp"

namespace tyke
{
/// 日志管理器单例访问宏
#define TYKE_LOG_INSTANCE TykeLog::GetInstance()

    /**
     * @brief 日志管理器类
     *
     * 负责初始化spdlog日志系统，配置日志文件轮转和日志级别。
     * 继承自Singleton<TykeLog>，确保全局只有一个实例。
     *
     * 使用示例：
     * @code
     *   TYKE_LOG_INSTANCE->Init("/var/log/tyke.log", "debug", 10, 5);
     *   LOG_INFO("server started");
     *   LOG_ERROR("connection failed: {}", error_msg);
     * @endcode
     */
    class TykeLog : public Singleton<TykeLog>
    {
        friend class Singleton<TykeLog>;

    public:
        /**
         * @brief 初始化日志系统
         * @param log_path 日志文件路径
         * @param log_level 日志级别（debug/info/warn/error）
         * @param file_size_mb 单个日志文件大小上限（MB）
         * @param file_count 日志文件轮转数量
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult Init(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                  uint32_t file_count);

        /**
         * @brief 设置日志级别
         * @param log_level 日志级别（debug/info/warn/error）
         */
        void SetLogLevel(const std::string& log_level) const;

        /**
         * @brief 停止日志系统
         *
         * 刷新所有日志缓冲区并关闭日志系统。
         */
        void Stop() const;

    private:
        TykeLog() = default;
        ~TykeLog() override = default;

        std::shared_ptr<spdlog::logger> tyke_logger_;  ///< spdlog日志器实例
    };
} // tyke