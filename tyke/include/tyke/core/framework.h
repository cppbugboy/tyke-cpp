/**
 * @file framework.h
 * @brief 框架主类声明。提供框架初始化、启动、停止及路由获取等生命周期管理接口。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <string>

#include "tyke/component/singleton.h"
#include "tyke/component/timing_wheel.h"
#include "tyke/ipc/ipc_server.h"
#include "request_router.h"
#include "response_router.h"

namespace tyke
{
    /** @brief 框架主类。提供初始化、启动、停止和路由获取接口。 */
    class Framework
    {
    public:
        Framework();

        ~Framework();

        /** @brief 设置线程池工作线程数。 */
        Framework& SetThreadPoolCount(uint32_t thread_pool_count);

        /** @brief 配置日志输出路径、级别、文件大小和数量。 */
        Framework& SetLogConfig(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                                uint32_t file_count);

        /** @brief 启动框架，绑定IPC监听UUID。 */
        [[nodiscard]] BoolResult Start(std::string_view listen_uuid);

        /** @brief 安全关闭框架，释放所有资源。 */
        void Shutdown();

        /** @brief 获取全局请求路由器单例。 */
        static RequestRouter& GetRequestRouter();

        /** @brief 获取全局响应路由器单例。 */
        static ResponseRouter& GetResponseRouter();

    private:
        uint32_t thread_pool_count_ = 0;
        std::string log_path_;
        std::string log_level_ = "info";
        uint32_t file_size_mb_ = 1024;
        uint32_t file_count_ = 5;
        TimerId cleanup_timer_id_ = TimingWheel::kInvalidTimerId;
        bool shutdown_ = false;  ///< 防重入标志，避免静态析构期间二次调用导致崩溃
    };


    Framework& App();
} // namespace tyke