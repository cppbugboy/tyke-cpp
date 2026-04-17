/**
 * @file tyke_framework.cpp
 * @brief Tyke框架主入口类实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现TykeFramework类的具体逻辑，包括框架初始化、启动和资源管理。
 */

#include "core/tyke_framework.h"

#include "common/tyke_utils.h"
#include "common/log_def.h"
#include "component/thread_pool.hpp"
#include "core/data_handler.h"
#include "core/tyke_log.h"
#include "ipc/ipc_server.h"
#include "controller/controller_registry.h"

namespace tyke
{
    /**
     * @brief 设置线程池工作线程数量
     * @param thread_pool_count 线程数量
     * @return 当前实例引用
     */
    TykeFramework& TykeFramework::SetThreadPoolCount(const uint32_t thread_pool_count)
    {
        thread_pool_count_ = thread_pool_count;
        return *this;
    }

    /**
     * @brief 配置日志系统
     * @param log_path 日志文件路径
     * @param log_level 日志级别
     * @param file_size_mb 单个日志文件大小上限（MB）
     * @param file_count 日志文件轮转数量
     * @return 当前实例引用
     */
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

    /**
     * @brief 启动框架
     * @param listen_uuid IPC服务监听的UUID标识
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult TykeFramework::Start(const std::string& listen_uuid) const
    {
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
        auto start_result = ipc_server_->Start(listen_uuid, DataHandler::DataCallback);
        if (!start_result)
        {
            LOG_ERROR("IPC server start failed: {}", start_result.error());
            return nonstd::make_unexpected("ipc server start failed: " + start_result.error());
        }

        LOG_INFO("Tyke framework started successfully");
        return true;
    }

    /**
     * @brief 获取请求路由器实例
     * @return 请求路由器指针
     */
    RequestRouter* TykeFramework::GetRequestRouter()
    {
        return REQUEST_ROUTER_INSTANCE;
    }

    /**
     * @brief 获取响应路由器实例
     * @return 响应路由器指针
     */
    ResponseRouter* TykeFramework::GetResponseRouter()
    {
        return RESPONSE_ROUTER_INSTANCE;
    }

    /**
     * @brief 构造函数
     *
     * 初始化IPC服务器和日志系统。
     */
    TykeFramework::TykeFramework()
    {
        ipc_server_ = std::unique_ptr<IpcServer>(new IpcServer());
        SetLogConfig(utils::GetTempDir() + "/" + "tyke.log", "info", 5, 5);
        LOG_DEBUG("TykeFramework constructed");
    }

    /**
     * @brief 析构函数
     *
     * 停止线程池和日志系统。
     */
    TykeFramework::~TykeFramework()
    {
        LOG_INFO("Tyke framework shutting down");
        THREAD_POOL_INSTANCE->Stop();
        TYKE_LOG_INSTANCE->Stop();
    }

    /**
     * @brief 获取框架单例实例的便捷函数
     * @return TykeFramework单例指针
     */
    TykeFramework* App()
    {
        return TykeFramework::GetInstance();
    }
} // tyke
