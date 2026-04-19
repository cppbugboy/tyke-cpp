#include "core/tyke_framework.h"

#include "common/tyke_utils.h"
#include "common/log_def.h"
#include "component/thread_pool.h"
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
        TYKE_LOG_INSTANCE->Init(log_path_, log_level_, file_size_mb_, file_count_);
        return *this;
    }
    BoolResult TykeFramework::Start(const std::string& listen_uuid) const
    {
        if (!TYKE_LOG_INSTANCE->IsInitialized())
        {
            TYKE_LOG_INSTANCE->Init(
                log_path_.empty() ? utils::GetTempDir() + "/tyke.log" : log_path_,
                log_level_, file_size_mb_, file_count_);
        }

        LOG_INFO("Tyke framework starting, listen_uuid={}", listen_uuid);

        // 初始化线程池
        THREAD_POOL_INSTANCE->Init(thread_pool_count_);
        LOG_DEBUG("Thread pool initialized with {} threads", thread_pool_count_);

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
        LOG_INFO("Tyke framework shutting down");
        THREAD_POOL_INSTANCE->Stop();
        TYKE_LOG_INSTANCE->Stop();
    }
    TykeFramework* App()
    {
        return TykeFramework::GetInstance();
    }
} // tyke
