/**
 * @file tyke_framework.cpp
 * @brief Tyke 框架主入口实现。
 * @author Nick
 * @date 2026/04/20
 */

#include "core/framework.h"

#include "common/log_def.h"
#include "common/tyke_utils.h"
#include "component/thread_pool.h"
#include "component/timing_wheel.h"
#include "core/data_handler.h"
#include "core/log_config.h"

namespace tyke
{
TykeFramework &TykeFramework::SetThreadPoolCount(const uint32_t thread_pool_count)
{
    thread_pool_count_ = thread_pool_count;
    return *this;
}

TykeFramework &TykeFramework::SetLogConfig(const std::string &log_path, const std::string &log_level,
                                           const uint32_t file_size_mb, const uint32_t file_count)
{
    log_path_     = log_path;
    log_level_    = log_level;
    file_size_mb_ = file_size_mb;
    file_count_   = file_count;
    if (!GetGlobalTykeLog().Init(log_path_, log_level_, file_size_mb_, file_count_))
    {
        fmt::print("Tyke framework initialization failed: {}", log_path_);
    }
    return *this;
}

BoolResult TykeFramework::Start(std::string_view listen_uuid) const
{
    if (!GetGlobalTykeLog().IsInitialized())
    {
        if (!GetGlobalTykeLog().Init(log_path_.empty() ? utils::GetTempDir() + "/tyke.log" : log_path_, log_level_,
                                     file_size_mb_, file_count_))
        {
            fmt::print("Tyke framework start failed: {}", log_path_);
            return false;
        }
    }

    LOG_INFO("Tyke framework starting, listen_uuid={}", listen_uuid);

    // 初始化线程池
    unsigned int thread_pool_count = thread_pool_count_;
    if (thread_pool_count == 0)
    {
        thread_pool_count = std::thread::hardware_concurrency();
    }
    if (thread_pool_count == 0)
    {
        thread_pool_count = 4;
    }
    GetGlobalThreadPool().Init(thread_pool_count);
    LOG_DEBUG("Thread pool initialized with {} threads", thread_pool_count_);

    // 初始化时间轮
    GetGlobalTimingWheel().Init();

    // 启动IPC服务器
    if (auto start_result = GetGlobalIpcServer().Start(listen_uuid, data_handler::DataCallback); !start_result)
    {
        LOG_ERROR("IPC server start failed: {}", start_result.error());
        return nonstd::make_unexpected("ipc server start failed: " + start_result.error());
    }

    LOG_INFO("Tyke framework started successfully");
    return true;
}

RequestRouter &TykeFramework::GetRequestRouter()
{ return GetGlobalRequestRouter(); }

ResponseRouter &TykeFramework::GetResponseRouter()
{ return GetGlobalResponseRouter(); }

TykeFramework::TykeFramework()
{
}

TykeFramework::~TykeFramework()
{ Shutdown(); }

void TykeFramework::Shutdown()
{
    LOG_INFO("Tyke framework shutting down");

    GetGlobalIpcServer().Stop();
    GetGlobalTimingWheel().Stop();
    GetGlobalThreadPool().Stop();
    GetGlobalTykeLog().Stop();
}

TykeFramework *App()
{
    static auto instance = new TykeFramework();
    return instance;
}
}// namespace tyke
