/**
 * @file tyke_framework.cpp
 * @brief Tyke 框架主入口实现。
 * @author Nick
 * @date 2026/04/20
 */

#include "core/tyke_framework.h"

#include "common/tyke_utils.h"
#include "common/log_def.h"
#include "component/thread_pool.h"
#include "component/timing_wheel.h"
#include "core/data_handler.h"
#include "core/tyke_log.h"
#include "ipc/ipc_server.h"
#include "controller/controller_registry.h"

namespace tyke
{
    TykeFramework& TykeFramework::SetThreadPoolCount(const uint32_t thread_pool_count)
    {
        thread_pool_count_ = thread_pool_count;
        return *this;
    }
    TykeFramework& TykeFramework::SetLogConfig(const std::string& log_path, const std::string& log_level,
                                               const uint32_t file_size_mb, const uint32_t file_count)
    {
        log_path_ = log_path;
        log_level_ = log_level;
        file_size_mb_ = file_size_mb;
        file_count_ = file_count;
        if (!TYKE_LOG_INSTANCE->Init(log_path_, log_level_, file_size_mb_, file_count_))
        {
            fmt::print("Tyke framework initialization failed: {}", log_path_);
        }
        return *this;
    }
    BoolResult TykeFramework::Start(std::string_view listen_uuid) const
    {
        if (!TYKE_LOG_INSTANCE->IsInitialized())
        {
            if (!TYKE_LOG_INSTANCE->Init(
                log_path_.empty() ? utils::GetTempDir() + "/tyke.log" : log_path_,
                log_level_, file_size_mb_, file_count_))
            {
                fmt::print("Tyke framework start failed: {}", log_path_);
                return false;
            }
        }

        LOG_INFO("Tyke framework starting, listen_uuid={}", listen_uuid);

        // 初始化线程池
        THREAD_POOL_INSTANCE->Init(thread_pool_count_);
        LOG_DEBUG("Thread pool initialized with {} threads", thread_pool_count_);

        TimingWheel::GetInstance()->Init();
        LOG_DEBUG("TimingWheel initialized");

        if (!ipc_server_)
        {
            LOG_ERROR("IPC server is not initialized");
            return nonstd::make_unexpected("ipc server is not initialized");
        }

        // 启动IPC服务器
        auto start_result = ipc_server_->Start(listen_uuid, data_handler::DataCallback);
        if (!start_result)
        {
            LOG_ERROR("IPC server start failed: {}", start_result.error());
            return nonstd::make_unexpected("ipc server start failed: " + start_result.error());
        }

        LOG_INFO("Tyke framework started successfully");
        return true;
    }
    RequestRouter* TykeFramework::GetRequestRouter()
    {
        return REQUEST_ROUTER_INSTANCE;
    }
    ResponseRouter* TykeFramework::GetResponseRouter()
    {
        return RESPONSE_ROUTER_INSTANCE;
    }
    TykeFramework::TykeFramework()
        : ipc_server_(std::unique_ptr<IpcServer>(new IpcServer()))
    {
    }
    TykeFramework::~TykeFramework()
    {
        Shutdown();
    }

    void TykeFramework::Shutdown()
    {
        LOG_INFO("Tyke framework shutting down");

        if (ipc_server_)
        {
            ipc_server_->Stop();
        }

        TimingWheel::GetInstance()->Stop();
        THREAD_POOL_INSTANCE->Stop();
        TYKE_LOG_INSTANCE->Stop();
    }
    TykeFramework* App()
    {
        return TykeFramework::GetInstance();
    }
} // tyke
