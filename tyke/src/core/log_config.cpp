/**
 * @file tyke_log.cpp
 * @brief 日志管理器实现。基于spdlog的日志系统，支持文件日志和日志级别配置。
 * @author Nick
 * @date 2026/04/19
 */

#include "core/log_config.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unordered_map>

#include "common/log_def.h"

namespace tyke
{
    bool TykeLog::IsInitialized() const
    {
        return tyke_logger_ != nullptr;
    }

    BoolResult TykeLog::Init(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                             uint32_t file_count)
    {
        if (tyke_logger_)
        {
            SetLogLevel(log_level);
            return true;
        }

        try
        {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace);

            const auto max_size = static_cast<size_t>(file_size_mb) * 1024 * 1024;
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_path, max_size, file_count);
            file_sink->set_level(spdlog::level::trace);

            tyke_logger_ = std::make_shared<spdlog::logger>("tyke", spdlog::sinks_init_list{console_sink, file_sink});
            tyke_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

            SetLogLevel(log_level);

            spdlog::register_logger(tyke_logger_);
            spdlog::set_default_logger(tyke_logger_);

            LOG_INFO("Tyke log system initialized, path={}, level={}, max_size={}MB, max_files={}", log_path, log_level,
                     file_size_mb, file_count);
            return true;
        }
        catch (const std::exception& e)
        {
            return nonstd::make_unexpected(std::string("Failed to initialize log system: ") + e.what());
        }
    }

    void TykeLog::SetLogLevel(const std::string& log_level) const
    {
        if (!tyke_logger_)
        {
            return;
        }

        static const std::unordered_map<std::string, spdlog::level::level_enum> level_map = {
            {"debug", spdlog::level::debug},
            {"info", spdlog::level::info},
            {"warn", spdlog::level::warn},
            {"error", spdlog::level::err}
        };
        auto it = level_map.find(log_level);
        tyke_logger_->set_level(it != level_map.end() ? it->second : spdlog::level::info);
        LOG_DEBUG("Log level set to: {}", log_level);
    }

    void TykeLog::Stop() const
    {
        if (tyke_logger_)
        {
            LOG_INFO("Tyke log system shutting down");
            tyke_logger_->flush();
            spdlog::shutdown();
        }
    }
} // namespace tyke