/**
 * @file tyke_log.h
 * @brief 日志管理器声明。基于spdlog的日志系统，支持文件日志和日志级别配置。
 * @author Nick
 * @date 2026/04/19
 */



#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <spdlog/logger.h>

#include "common/tyke_result.h"
#include "component/singleton.h"

namespace tyke
{

#define TYKE_LOG_INSTANCE TykeLog::GetInstance()

    
    class TykeLog : public Singleton<TykeLog>
    {
        friend class Singleton<TykeLog>;

    public:
        
        BoolResult Init(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                  uint32_t file_count);

        bool IsInitialized() const { return tyke_logger_ != nullptr; }

        
        void SetLogLevel(const std::string& log_level) const;

        
        void Stop() const;

    private:
        TykeLog() = default;
        ~TykeLog() override = default;

        std::shared_ptr<spdlog::logger> tyke_logger_;
    };
}