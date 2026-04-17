/**
 * @file tyke_framework.h
 * @brief Tyke框架主入口类
 * @author Nick
 * @date 2026/04/17
 *
 * TykeFramework是框架的核心入口类，采用单例模式实现。
 * 负责初始化和管理框架的各个组件，包括IPC服务器、线程池、日志系统等。
 *
 * 使用示例：
 * @code
 *   auto app = tyke::App();
 *   app->SetThreadPoolCount(8)
 *       ->SetLogConfig("/var/log/tyke.log", "debug", 10, 5);
 *   auto result = app->Start("my-server-uuid");
 *   if (!result) {
 *       // 处理启动失败
 *   }
 * @endcode
 */

#pragma once

#include <string>

#include "request_router.h"
#include "response_router.h"
#include "common/tyke_def.h"
#include "common/tyke_result.h"
#include "component/singleton.hpp"
#include "ipc/ipc_server.h"

namespace tyke
{
    /**
     * @brief Tyke框架主类
     *
     * 框架的核心入口类，负责协调各个组件的初始化和生命周期管理。
     * 继承自Singleton<TykeFramework>，确保全局只有一个实例。
     *
     * 主要职责：
     * - 管理IPC服务器的启动和停止
     * - 配置线程池大小
     * - 初始化日志系统
     * - 提供请求/响应路由器的访问接口
     */
    class TykeFramework : public Singleton<TykeFramework>
    {
        friend class Singleton<TykeFramework>;

    public:
        /**
         * @brief 设置线程池工作线程数量
         * @param thread_pool_count 线程数量，默认为4
         * @return 返回当前实例引用，支持链式调用
         */
        TykeFramework& SetThreadPoolCount(uint32_t thread_pool_count);

        /**
         * @brief 配置日志系统
         * @param log_path 日志文件路径，为空则使用临时目录
         * @param log_level 日志级别（debug/info/warn/error）
         * @param file_size_mb 单个日志文件大小上限（MB）
         * @param file_count 日志文件轮转数量
         * @return 返回当前实例引用，支持链式调用
         */
        TykeFramework& SetLogConfig(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                                    uint32_t file_count);

        /**
         * @brief 启动框架
         * @param listen_uuid IPC服务监听的UUID标识
         * @return 成功返回true，失败返回错误信息
         *
         * 启动流程：
         * 1. 初始化线程池
         * 2. 启动IPC服务器
         * 3. 开始监听客户端连接
         */
        BoolResult Start(const std::string& listen_uuid) const;

        /**
         * @brief 获取请求路由器实例
         * @return 请求路由器指针
         */
        RequestRouter* GetRequestRouter();

        /**
         * @brief 获取响应路由器实例
         * @return 响应路由器指针
         */
        ResponseRouter* GetResponseRouter();

    private:
        TykeFramework();
        ~TykeFramework() override;

        uint32_t thread_pool_count_ = 4;       ///< 线程池工作线程数量
        std::string log_path_;                  ///< 日志文件路径
        std::string log_level_ = "info";        ///< 日志级别
        uint32_t file_size_mb_ = 1024;          ///< 单个日志文件大小上限（MB）
        uint32_t file_count_ = 5;               ///< 日志文件轮转数量
        std::unique_ptr<IpcServer> ipc_server_ = nullptr;  ///< IPC服务器实例
    };

    /**
     * @brief 获取框架单例实例的便捷函数
     * @return TykeFramework单例指针
     */
    TykeFramework* App();
} // namespace tyke