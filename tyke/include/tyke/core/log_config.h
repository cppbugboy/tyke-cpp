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
    class LogConfig
    {
    public:
        LogConfig() = default;
        ~LogConfig() = default;

        BoolResult Init(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                        uint32_t file_count);

        [[nodiscard]] bool IsInitialized() const;


        void SetLogLevel(const std::string& log_level) const;


        void Stop() const;

    private:
        std::shared_ptr<spdlog::logger> tyke_logger_;
    };

    inline LogConfig& GetGlobalLogConfig()
    {
        static LogConfig instance;
        return instance;
    }
} // namespace tyke
