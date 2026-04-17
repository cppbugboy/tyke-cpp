/**
 * @file tyke_log.cpp
 * @brief Tyke日志管理器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现TykeLog类的具体逻辑，初始化和管理spdlog日志系统。
 */

#include "core/tyke_log.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "common/log_def.h"

namespace tyke
{
    /**
     * @brief 初始化日志系统
     * @param log_path 日志文件路径
     * @param log_level 日志级别（debug/info/warn/error）
     * @param file_size_mb 单个日志文件大小上限（MB）
     * @param file_count 日志文件轮转数量
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult TykeLog::Init(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                             uint32_t file_count)
    {
        try
        {
            // 创建控制台输出sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace);

            // 创建文件输出sink（带轮转）
            const auto max_size = static_cast<size_t>(file_size_mb) * 1024 * 1024;
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_path, max_size, file_count);
            file_sink->set_level(spdlog::level::trace);

            // 创建多sink日志器
            tyke_logger_ = std::make_shared<spdlog::logger>("tyke", spdlog::sinks_init_list{console_sink, file_sink});
            tyke_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

            // 设置日志级别
            SetLogLevel(log_level);

            // 注册为默认日志器
            spdlog::register_logger(tyke_logger_);
            spdlog::set_default_logger(tyke_logger_);

            LOG_INFO("Tyke log system initialized, path={}, level={}, max_size={}MB, max_files={}",
                     log_path, log_level, file_size_mb, file_count);
            return true;
        }
        catch (const std::exception& e)
        {
            return nonstd::make_unexpected(std::string("Failed to initialize log system: ") + e.what());
        }
    }

    /**
     * @brief 设置日志级别
     * @param log_level 日志级别（debug/info/warn/error）
     */
    void TykeLog::SetLogLevel(const std::string& log_level) const
    {
        if (!tyke_logger_)
        {
            return;
        }

        spdlog::level::level_enum level = spdlog::level::info;
        if (log_level == "debug")
        {
            level = spdlog::level::debug;
        }
        else if (log_level == "info")
        {
            level = spdlog::level::info;
        }
        else if (log_level == "warn")
        {
            level = spdlog::level::warn;
        }
        else if (log_level == "error")
        {
            level = spdlog::level::err;
        }

        tyke_logger_->set_level(level);
        LOG_DEBUG("Log level set to: {}", log_level);
    }

    /**
     * @brief 停止日志系统
     *
     * 刷新所有日志缓冲区并关闭日志系统。
     */
    void TykeLog::Stop() const
    {
        if (tyke_logger_)
        {
            LOG_INFO("Tyke log system shutting down");
            tyke_logger_->flush();
            spdlog::shutdown();
        }
    }
} // tyke
