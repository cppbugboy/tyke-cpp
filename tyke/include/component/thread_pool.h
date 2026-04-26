/**
 * @file thread_pool.h
 * @brief 高性能优先级线程池（C++17），支持高/中/低三级任务优先级调度。
 *
 * 本模块实现了一个支持优先级调度的高性能线程池，适用于需要区分任务重要性的场景。
 *
 * @section features 主要特性
 * - 三分离优先级队列（High/Medium/Low），严格按优先级调度
 * - 高优先级任务始终优先于中/低优先级任务获取线程资源
 * - 使用互斥锁+条件变量保证线程安全的优先级调度
 * - 支持动态线程数调整（自动扩缩容）
 * - 支持优雅降级策略（队列满时同步执行）
 * - 完善的指标统计，含各优先级队列状态监控
 *
 * @section usage 使用示例
 * @code
 * // 获取全局线程池
 * auto& pool = tyke::GetGlobalThreadPool();
 * pool.Init(4);
 *
 * // 提交高优先级任务
 * auto future = pool.EnqueueWithPriority(tyke::TaskPriority::High, []() {
 *     return 42;
 * });
 *
 * // 提交默认优先级任务（Medium）
 * pool.Enqueue([]() { // do something });
 *
 * // 优雅停止
 * pool.Stop(true);
 * @endcode
 *
 * @author Nick
 * @date 2026/04/26
 * @version 2.0
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

namespace tyke
{
    /**
     * @brief 任务优先级枚举
     *
     * 定义三个优先级级别，数值越大优先级越高。
     * 工作线程始终按 High -> Medium -> Low 的顺序从队列中获取任务。
     */
    enum class TaskPriority : int
    {
        Low = 0, ///< 低优先级：后台任务、日志清理等非关键任务
        Medium = 1, ///< 中优先级（默认）：普通业务逻辑处理
        High = 2 ///< 高优先级：紧急任务、关键路径操作
    };

    /**
     * @brief 线程池运行指标
     *
     * 用于监控线程池的运行状态和性能指标，支持通过 GetMetrics() 获取。
     */
    struct ThreadPoolMetrics
    {
        uint64_t total_tasks_submitted = 0; ///< 累计提交任务数
        uint64_t total_tasks_completed = 0; ///< 累计完成任务数
        uint64_t total_tasks_dropped = 0; ///< 累计丢弃任务数（池停止时提交）
        uint64_t total_tasks_timeout = 0; ///< 累计超时任务数
        int32_t current_queue_size = 0; ///< 当前队列总大小
        int32_t high_queue_size = 0; ///< 高优先级队列大小
        int32_t medium_queue_size = 0; ///< 中优先级队列大小
        int32_t low_queue_size = 0; ///< 低优先级队列大小
        int32_t current_active_workers = 0; ///< 当前活跃工作线程数
        int32_t current_idle_workers = 0; ///< 当前空闲工作线程数
        int32_t peak_queue_size = 0; ///< 队列大小峰值
        int32_t peak_active_workers = 0; ///< 活跃线程数峰值
        uint64_t average_task_latency_ns = 0; ///< 平均任务延迟（纳秒）
        uint64_t queue_full_reject_count = 0; ///< 队列满拒绝次数
        uint64_t scale_up_count = 0; ///< 扩容次数
        uint64_t scale_down_count = 0; ///< 缩容次数
    };

    /**
     * @brief 线程池配置参数
     *
     * 用于初始化线程池的各项参数，包括线程数、队列容量、自动扩缩容策略等。
     */
    struct ThreadPoolConfig
    {
        size_t initial_workers = 0; ///< 初始工作线程数，0表示自动检测CPU核心数
        size_t initial_queue_capacity = 4096; ///< 初始队列容量
        size_t max_workers = 0; ///< 最大工作线程数，0表示initial_workers*8
        size_t min_workers = 1; ///< 最小工作线程数
        bool enable_auto_scale = true; ///< 是否启用自动扩缩容
        double scale_threshold = 0.8; ///< 扩容阈值（负载因子）
        double shrink_threshold = 0.2; ///< 缩容阈值（负载因子）
        std::chrono::milliseconds scale_interval{5000}; ///< 扩缩容检查间隔
        std::chrono::milliseconds scale_up_cooldown{2000}; ///< 扩容冷却时间
        std::chrono::milliseconds scale_down_cooldown{10000}; ///< 缩容冷却时间
        size_t scale_up_step = 2; ///< 每次扩容增加的线程数
        size_t scale_down_step = 1; ///< 每次缩容减少的线程数
        bool enable_metrics = true; ///< 是否启用指标统计
        std::chrono::milliseconds task_timeout{30000}; ///< 任务超时时间
    };

    /**
     * @brief 生成默认线程池配置
     *
     * 根据系统CPU核心数自动设置合理的初始参数。
     *
     * @return ThreadPoolConfig 默认配置对象
     *
     * @code
     * auto config = tyke::DefaultThreadPoolConfig();
     * config.initial_workers = 8;  // 可根据需要覆盖
     * pool.InitWithConfig(config);
     * @endcode
     */
    inline ThreadPoolConfig DefaultThreadPoolConfig()
    {
        ThreadPoolConfig config;
        const auto hw_threads = std::thread::hardware_concurrency();
        config.initial_workers = hw_threads > 0 ? hw_threads : 4;
        config.max_workers = config.initial_workers * 8;
        config.min_workers = config.initial_workers;
        return config;
    }

    /**
     * @brief 高性能优先级线程池类
     *
     * 实现支持三级优先级调度的线程池，使用三分离队列确保高优先级任务
     * 始终优先于低优先级任务执行。线程安全，支持动态扩缩容。
     *
     * @note 线程池不可复制，建议通过 GetGlobalThreadPool() 使用全局单例。
     */
    class ThreadPool
    {
    public:
        /**
         * @brief 构造函数
         *
         * 构造一个未初始化的线程池实例，需要调用 Init() 或 InitWithConfig() 启动。
         */
        explicit ThreadPool();

        /**
         * @brief 析构函数
         *
         * 自动调用 Stop(true) 优雅停止线程池。
         */
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /**
         * @brief 使用默认配置初始化线程池
         *
         * @param threads 工作线程数量，0表示自动检测CPU核心数
         *
         * @code
         * ThreadPool pool;
         * pool.Init(4);
         * @endcode
         */
        void Init(size_t threads);

        /**
         * @brief 使用自定义配置初始化线程池
         *
         * @param config 线程池配置参数
         *
         * @code
         * ThreadPool pool;
         * auto config = DefaultThreadPoolConfig();
         * config.enable_auto_scale = false;
         * pool.InitWithConfig(config);
         * @endcode
         */
        void InitWithConfig(const ThreadPoolConfig& config);

        /**
         * @brief 停止线程池
         *
         * @param wait_for_tasks 是否等待队列中的任务执行完成
         *                        true: 优雅停止，等待所有任务完成
         *                        false: 立即停止，丢弃队列中的任务
         */
        void Stop(bool wait_for_tasks = true);

        /**
         * @brief 提交任务（默认Medium优先级）
         *
         * @tparam F 可调用对象类型
         * @tparam Args 参数类型
         * @param f 可调用对象
         * @param args 传递给可调用对象的参数
         * @return std::optional<std::future<返回类型>> 成功返回future，失败返回nullopt
         *
         * @code
         * auto future = pool.Enqueue([](int x) { return x * 2; }, 21);
         * if (future) {
         *     int result = future.value().get();  // result = 42
         * }
         * @endcode
         */
        template <class F, class... Args>
        auto Enqueue(F&& f, Args&&... args) -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
        {
            return EnqueueWithPriority(TaskPriority::Medium, std::forward<F>(f), std::forward<Args>(args)...);
        }

        /**
         * @brief 提交指定优先级的任务
         *
         * @tparam F 可调用对象类型
         * @tparam Args 参数类型
         * @param priority 任务优先级
         * @param f 可调用对象
         * @param args 传递给可调用对象的参数
         * @return std::optional<std::future<返回类型>> 成功返回future，失败返回nullopt
         *
         * @code
         * // 高优先级任务
         * pool.EnqueueWithPriority(TaskPriority::High, []() {
         *     handleUrgentRequest();
         * });
         *
         * // 低优先级后台任务
         * pool.EnqueueWithPriority(TaskPriority::Low, []() {
         *     cleanupOldData();
         * });
         * @endcode
         */
        template <class F, class... Args>
        auto EnqueueWithPriority(TaskPriority priority, F&& f, Args&&... args)
            -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
        {
            if (stop_.load(std::memory_order_acquire))
            {
                RecordTaskDropped();
                return std::nullopt;
            }

            using return_type = std::invoke_result_t<F, Args...>;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                [fn = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> return_type
                {
                    return std::apply(fn, std::move(tup));
                });

            std::future<return_type> result = task->get_future();

            TaskWrapper wrapper;
            wrapper.task = [task]()
            {
                (*task)();
            };
            wrapper.enqueue_time = std::chrono::steady_clock::now();
            wrapper.priority = priority;

            {
                std::lock_guard<std::mutex> lock(queue_mutex_);

                if (TotalQueueSize() >= static_cast<int32_t>(config_.initial_queue_capacity))
                {
                    RecordQueueFull();
                    return std::nullopt;
                }

                GetQueue(priority).push(std::move(wrapper));
            }

            queue_cv_.notify_one();
            RecordTaskSubmitted();
            return result;
        }

        /**
         * @brief 提交任务，带超时等待
         *
         * 尝试将任务入队，如果队列满则等待最多 timeout 时间。
         * 如果在超时时间内队列有空位，任务将被入队；
         * 如果超时，则返回 nullopt 并记录超时指标。
         *
         * @param timeout 最大等待时间
         * @param f 可调用对象
         * @param args 参数
         * @return std::optional<std::future<返回类型>> 成功返回future，超时返回nullopt
         *
         * @code
         * auto future = pool.EnqueueWithTimeout(std::chrono::milliseconds(100), []() {
         *     return 42;
         * });
         * if (future) {
         *     int result = future.value().get();
         * } else {
         *     // 入队超时
         * }
         * @endcode
         */
        template <class F, class... Args>
        auto EnqueueWithTimeout(std::chrono::milliseconds timeout, F&& f, Args&&... args)
            -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
        {
            if (stop_.load(std::memory_order_acquire))
            {
                RecordTaskDropped();
                return std::nullopt;
            }

            using return_type = std::invoke_result_t<F, Args...>;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                [fn = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> return_type
                {
                    return std::apply(fn, std::move(tup));
                });

            std::future<return_type> result = task->get_future();

            TaskWrapper wrapper;
            wrapper.task = [task]()
            {
                (*task)();
            };
            wrapper.enqueue_time = std::chrono::steady_clock::now();
            wrapper.priority = TaskPriority::Medium;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);

                if (TotalQueueSize() >= static_cast<int32_t>(config_.initial_queue_capacity))
                {
                    if (!queue_cv_.wait_for(lock, timeout,
                                            [this]()
                                            {
                                                return stop_.load(std::memory_order_acquire) ||
                                                    TotalQueueSize() < static_cast<int32_t>(config_.
                                                        initial_queue_capacity);
                                            }))
                    {
                        RecordTaskTimeout();
                        return std::nullopt;
                    }

                    if (stop_.load(std::memory_order_acquire))
                    {
                        RecordTaskDropped();
                        return std::nullopt;
                    }
                }

                GetQueue(TaskPriority::Medium).push(std::move(wrapper));
            }

            queue_cv_.notify_one();
            RecordTaskSubmitted();
            return result;
        }

        /**
         * @brief 提交任务，失败则在当前线程同步执行
         *
         * 当线程池停止或队列满时，直接在调用线程执行任务，确保任务不丢失。
         *
         * @tparam F 可调用对象类型
         * @tparam Args 参数类型
         * @param f 可调用对象
         * @param args 参数
         * @return std::optional<std::future<返回类型>> 入队成功返回future，同步执行返回nullopt
         *
         * @code
         * // 保证任务一定会执行
         * pool.EnqueueOrExecute([]() {
         *     criticalOperation();
         * });
         * @endcode
         */
        template <class F, class... Args>
        auto EnqueueOrExecute(F&& f, Args&&... args) -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
        {
            if (stop_.load(std::memory_order_acquire))
            {
                std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
                return std::nullopt;
            }

            auto result = Enqueue(std::forward<F>(f), std::forward<Args>(args)...);
            if (!result)
            {
                std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
            }
            return result;
        }

        /**
         * @brief 获取工作线程数量
         * @return size_t 当前工作线程数
         */
        size_t WorkerCount() const { return workers_.size(); }

        /**
         * @brief 获取正在执行的任务数量
         * @return size_t 活跃任务数
         */
        size_t ActiveTaskCount() const { return static_cast<size_t>(active_tasks_.load()); }

        /**
         * @brief 获取队列总大小
         * @return size_t 队列中待执行任务数
         */
        size_t QueueSize() const { return static_cast<size_t>(queue_size_.load()); }

        /**
         * @brief 获取指定优先级队列的大小
         *
         * @param priority 任务优先级
         * @return size_t 该优先级队列中的任务数
         */
        size_t QueueSizeByPriority(TaskPriority priority) const;

        /**
         * @brief 检查线程池是否正在运行
         * @return true 线程池运行中
         * @return false 线程池已停止
         */
        bool IsRunning() const { return !stop_.load(std::memory_order_acquire); }

        /**
         * @brief 获取线程池运行指标
         * @return ThreadPoolMetrics 指标快照
         */
        ThreadPoolMetrics GetMetrics() const;

        /**
         * @brief 根据名称字符串获取优先级枚举值
         *
         * @param priority_name 优先级名称（"high"/"High"/"HIGH"/"low"/"Low"/"LOW"/其他）
         * @return TaskPriority 对应的优先级枚举，未知名称返回Medium
         */
        TaskPriority GetTaskPriority(const std::string& priority_name) const;

        /**
         * @brief 设置任务异常处理器
         *
         * 当任务抛出异常时，调用此处理器进行处理。
         *
         * @param handler 异常处理函数，接收异常消息字符串
         */
        void SetPanicHandler(std::function<void(const std::string&)> handler) { panic_handler_ = std::move(handler); }

    private:
        /**
         * @brief 任务包装器内部结构
         */
        struct TaskWrapper
        {
            std::function<void()> task; ///< 任务函数
            std::chrono::steady_clock::time_point enqueue_time; ///< 入队时间（用于延迟统计）
            TaskPriority priority = TaskPriority::Medium; ///< 任务优先级
        };

        /**
         * @brief 启动指定数量的工作线程
         * @param count 线程数量
         */
        void StartWorkers(size_t count);

        /**
         * @brief 工作线程主循环
         *
         * 循环等待并执行任务，按 High -> Medium -> Low 顺序获取任务。
         */
        void WorkerLoop();

        /**
         * @brief 执行单个任务
         * @param wrapper 任务包装器
         */
        void ExecuteTask(TaskWrapper& wrapper);

        /**
         * @brief 启动自动扩缩容检查线程
         */
        void StartScalingLoop();

        /**
         * @brief 检查并执行扩缩容
         */
        void CheckAndScale();

        /**
         * @brief 记录任务提交
         */
        void RecordTaskSubmitted();

        /**
         * @brief 记录任务完成并更新延迟统计
         * @param wrapper 任务包装器
         */
        void RecordTaskCompleted(const TaskWrapper& wrapper);

        /**
         * @brief 记录任务丢弃
         */
        void RecordTaskDropped();

        /**
         * @brief 记录任务超时
         */
        void RecordTaskTimeout();

        /**
         * @brief 记录队列满
         */
        void RecordQueueFull();

        /**
         * @brief 更新峰值指标
         */
        void UpdatePeakMetrics();

        /**
         * @brief 获取指定优先级对应的队列引用
         * @param priority 优先级
         * @return std::queue<TaskWrapper>& 队列引用
         */
        std::queue<TaskWrapper>& GetQueue(TaskPriority priority);

        /**
         * @brief 获取指定优先级对应的队列常量引用
         * @param priority 优先级
         * @return const std::queue<TaskWrapper>& 队列常量引用
         */
        const std::queue<TaskWrapper>& GetQueue(TaskPriority priority) const;

        /**
         * @brief 计算三个队列的总大小
         * @return int32_t 总任务数
         * @note 调用者必须持有 queue_mutex_
         */
        int32_t TotalQueueSize() const;

        std::queue<TaskWrapper> high_queue_; ///< 高优先级任务队列
        std::queue<TaskWrapper> medium_queue_; ///< 中优先级任务队列
        std::queue<TaskWrapper> low_queue_; ///< 低优先级任务队列
        mutable std::mutex queue_mutex_; ///< 队列互斥锁
        std::condition_variable queue_cv_; ///< 队列条件变量

        std::vector<std::thread> workers_; ///< 工作线程集合
        std::atomic<bool> stop_{false}; ///< 停止标志
        std::atomic<int32_t> active_tasks_{0}; ///< 活跃任务计数
        std::atomic<int32_t> idle_workers_{0}; ///< 空闲线程计数
        std::atomic<int32_t> queue_size_{0}; ///< 队列大小计数

        ThreadPoolConfig config_; ///< 配置参数
        std::thread scale_thread_; ///< 扩缩容检查线程
        std::chrono::steady_clock::time_point last_scale_up_; ///< 上次扩容时间
        std::chrono::steady_clock::time_point last_scale_down_; ///< 上次缩容时间

        mutable std::mutex metrics_mutex_; ///< 指标互斥锁
        ThreadPoolMetrics metrics_; ///< 运行指标

        std::function<void(const std::string&)> panic_handler_; ///< 异常处理器
    };

    /**
     * @brief 获取全局线程池单例
     *
     * 返回进程级别的线程池单例，推荐在框架中使用。
     *
     * @return ThreadPool& 全局线程池引用
     *
     * @code
     * auto& pool = tyke::GetGlobalThreadPool();
     * pool.Init(4);
     */
    ThreadPool& GetGlobalThreadPool();
} // namespace tyke
