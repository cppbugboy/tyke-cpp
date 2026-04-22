
#pragma once

#include "core/request_router.h"
#include "core/response_router.h"
#include "component/object_pool.h"
#include "component/thread_pool.h"
#include "component/timing_wheel.h"
#include "core/tyke_log.h"
#include "core/tyke_request.h"
#include "ipc/connection_pool_factory.h"
#include "ipc/ipc_server.h"

namespace tyke
{
    inline std::shared_ptr<ThreadPool> GetThreadPoolSingleton()
    {
        static auto instance = std::make_shared<ThreadPool>();
        return instance;
    }

    inline std::shared_ptr<TimingWheel> GetTimingWheelSingleton()
    {
        static auto instance = std::make_shared<TimingWheel>();
        return instance;
    }


    inline std::shared_ptr<ObjectPool<TykeRequest>> GetTykeRequestSingleton()
    {
        static auto instance = std::make_shared<ObjectPool<TykeRequest>>();
        return instance;
    }

    inline std::shared_ptr<ObjectPool<TykeResponse>> GetTykeResponseSingleton()
    {
        static auto instance = std::make_shared<ObjectPool<TykeResponse>>();
        return instance;
    }

    inline std::shared_ptr<RequestRouter> GetRequestRouterSingleton()
    {
        static auto instance = std::make_shared<RequestRouter>();
        return instance;
    }

    inline std::shared_ptr<ResponseRouter> GetResponseRouterSingleton()
    {
        static auto instance = std::make_shared<ResponseRouter>();
        return instance;
    }

    inline std::shared_ptr<ConnectionPoolFactory> GetConnectionPoolFactorySingleton()
    {
        static auto instance = std::make_shared<ConnectionPoolFactory>();
        return instance;
    }

    inline std::shared_ptr<TykeLog> GetTykeLogSingleton()
    {
        static auto instance = std::make_shared<TykeLog>();
        return instance;
    }

    inline std::shared_ptr<IpcServer> GetIpcServerSingleton()
    {
        static auto instance = std::make_shared<IpcServer>();
        return instance;
    }

}
