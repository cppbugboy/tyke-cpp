/**
 * @file log_config.h
 * @brief 日志管理器声明。基于spdlog的日志系统，支持文件日志和日志级别配置。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <cstdint>
#include <memory>
#include <spdlog/logger.h>
#include <string>

#include "tyke/common/tyke_def.h"
#include "tyke/component/singleton.h"

namespace tyke
{
    /** @brief 日志配置管理器。基于 spdlog，支持文件日志和运行时级别切换。 */
    class LogConfig
    {
    public:
        LogConfig() = default;
        ~LogConfig() = default;

        /** @brief 初始化日志系统。
         * @param log_path 日志文件路径。
         * @param log_level 日志级别字符串（如 "debug", "info", "warn", "error"）。
         * @param file_size_mb 单个日志文件最大大小(MB)。
         * @param file_count 滚动保留的日志文件数量。
         * @return 初始化成功返回 true。
         */
        BoolResult Init(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                        uint32_t file_count);

        /** @brief 检查日志是否已初始化。 */
        [[nodiscard]] bool IsInitialized() const;

        /** @brief 动态修改日志级别。 */
        void SetLogLevel(const std::string& log_level) const;

        /** @brief 停止日志系统，刷新并关闭所有日志文件。 */
        void Stop() const;

    private:
        std::shared_ptr<spdlog::logger> tyke_logger_;
    };

    /** @brief 获取全局日志配置单例。 */
    inline LogConfig& GetGlobalLogConfig()
    {
        static LogConfig instance;
        return instance;
    }
} // namespace tyke
