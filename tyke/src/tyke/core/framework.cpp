/**
 * @file framework.cpp
 * @brief Tyke 框架主入口实现。负责线程池、时间轮、IPC 服务端和日志系统的生命周期管理。
 * @author Nick
 * @date 2026/04/20
 */

#include "tyke/core/framework.h"

#include "tyke/common/log_def.h"
#include "tyke/common/tyke_def.h"
#include "tyke/common/tyke_utils.h"
#include "tyke/component/thread_pool.h"
#include "tyke/component/timing_wheel.h"
#include "tyke/core/data_handler.h"
#include "tyke/core/log_config.h"
#include "tyke/core/request_stub.h"

namespace tyke
{
    /** @brief 设置线程池数量，在 Start() 前调用。默认检测硬件并发数。 */
    Framework& Framework::SetThreadPoolCount(const uint32_t thread_pool_count)
    {
        thread_pool_count_ = thread_pool_count;
        return *this;
    }

    /** @brief 配置日志系统路径、级别、文件大小和数量，立即初始化日志。 */
    Framework& Framework::SetLogConfig(const std::string& log_path, const std::string& log_level,
                                       const uint32_t file_size_mb, const uint32_t file_count)
    {
        log_path_ = log_path;
        log_level_ = log_level;
        file_size_mb_ = file_size_mb;
        file_count_ = file_count;
        if (!GetGlobalLogConfig().Init(log_path_, log_level_, file_size_mb_, file_count_))
        {
            fmt::print("Tyke framework initialization failed: {}", log_path_);
        }
        return *this;
    }

    /**
     * @brief 启动框架：初始化线程池、时间轮、注册 stub 清理任务、启动 IPC 服务端。
     *
     * @param listen_uuid 服务端监听 UUID，格式必须为合法的 36 字符 UUID。
     * @note 若日志未初始化则使用默认配置（临时目录下的 tyke.log）。
     * @note 线程池数量为 0 时自动检测硬件并发数，检测失败则默认为 4。
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult Framework::Start(std::string_view listen_uuid)
    {
        if (!utils::IsValidUUID(listen_uuid))
        {
            return nonstd::make_unexpected("uuid is invalid");
        }

        if (!GetGlobalLogConfig().IsInitialized())
        {
            if (!GetGlobalLogConfig().Init(log_path_.empty() ? utils::GetTempDir() + "/tyke.log" : log_path_,
                                           log_level_,
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

        // 注册周期性超时清理任务，间隔为默认 stub 超时的 1/4
        const uint32_t cleanup_interval_ms = kDefaultStubTimeoutMs / 4;
        cleanup_timer_id_ = GetGlobalTimingWheel().AddRepeatedTask(cleanup_interval_ms, cleanup_interval_ms,
                                                                   []()
                                                                   {
                                                                       stub::CleanupExpiredFuncs();
                                                                       stub::CleanupExpiredFutures();
                                                                   });
        LOG_DEBUG("Stub cleanup task registered, interval={}ms, timer_id={}", cleanup_interval_ms, cleanup_timer_id_);

        // 启动IPC服务器
        if (auto start_result = GetGlobalIpcServer().Start(listen_uuid, data_handler::DataCallback); !start_result)
        {
            LOG_ERROR("IPC server start failed: {}", start_result.error());
            return nonstd::make_unexpected("ipc server start failed: " + start_result.error());
        }

        LOG_INFO("Tyke framework started successfully");
        return true;
    }

    RequestRouter& Framework::GetRequestRouter()
    {
        return GetGlobalRequestRouter();
    }

    ResponseRouter& Framework::GetResponseRouter()
    {
        return GetGlobalResponseRouter();
    }

    Framework::Framework()
    {
    }

    Framework::~Framework()
    {
        Shutdown();
    }

    /**
     * @brief 关闭框架所有子系统。
     *
     * 关闭顺序至关重要：
     * 1. 先取消 stub 清理定时器
     * 2. 停时间轮（取消定时回调，停止 tick 线程）
     * 3. 停线程池（等待所有任务完成，确保不再有任务访问 IPC 服务端）
     * 4. 停 IPC 服务端（此时所有使用者已退出）
     * 5. 日志系统最后关闭（保证前面步骤的日志能正常输出）
     *
     * @warning 关闭顺序不可随意调整，否则可能产生 use-after-free 或日志丢失。
     */
    void Framework::Shutdown()
    {
        if (shutdown_)
            return;
        shutdown_ = true;

        LOG_INFO("Tyke framework shutting down");

        if (cleanup_timer_id_ != TimingWheel::kInvalidTimerId)
        {
            GetGlobalTimingWheel().CancelTask(cleanup_timer_id_);
            cleanup_timer_id_ = TimingWheel::kInvalidTimerId;
        }

        GetGlobalTimingWheel().Stop();
        GetGlobalThreadPool().Stop();
        GetGlobalIpcServer().Stop();
        GetGlobalLogConfig().Stop();
    }

    /** @brief 获取全局 Framework 单例。 */
    Framework& App()
    {
        static Framework instance;
        return instance;
    }
} // namespace tyke
