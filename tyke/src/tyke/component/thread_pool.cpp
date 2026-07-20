/**
 * @file thread_pool.cpp
 * @brief 高性能优先级线程池实现，支持高/中/低三级任务优先级调度。
 *
 * 本文件实现了 ThreadPool 类的所有成员函数，包括：
 * - 线程池的初始化与停止
 * - 工作线程的任务获取与执行（按优先级顺序）
 * - 自动扩缩容机制
 * - 运行指标统计
 *
 * @see thread_pool.h
 * @author Nick
 * @date 2026/04/26
 * @version 2.0
 */

#include "tyke/component/thread_pool.h"

#include "tyke/common/log_def.h"

#include <algorithm>

namespace tyke
{
    /**
     * @brief 构造函数实现
     *
     * 创建一个未初始化的线程池实例。
     * 实际的线程创建在 Init() 或 InitWithConfig() 中进行。
     */
    ThreadPool::ThreadPool()
    {
    }

    /**
     * @brief 析构函数实现
     *
     * 自动调用 Stop(true) 确保线程池优雅停止，
     * 等待所有队列中的任务执行完成后再销毁。
     */
    ThreadPool::~ThreadPool()
    {
        Stop(true);
    }

    /**
     * @brief 使用默认配置初始化线程池
     *
     * 根据指定的线程数创建线程池，其他参数使用默认值。
     * 如果 threads 为 0，则自动检测 CPU 核心数。
     *
     * @param threads 工作线程数量
     */
    void ThreadPool::Init(size_t threads)
    {
        auto config = DefaultThreadPoolConfig();
        if (threads > 0)
        {
            config.initial_workers = threads;
            config.max_workers = std::max(threads * 8, threads);
            config.min_workers = threads;
        }
        InitWithConfig(config);
    }

    /**
     * @brief 使用自定义配置初始化线程池
     *
     * 根据配置参数创建工作线程，并可选地启动自动扩缩容线程。
     * 此方法只能调用一次，重复调用会被忽略。
     *
     * @param config 线程池配置参数
     */
    void ThreadPool::InitWithConfig(const ThreadPoolConfig& config)
    {
        if (!workers_.empty())
        {
            LOG_WARN("ThreadPool already initialized");
            return;
        }

        config_ = config;
        if (config_.initial_workers == 0)
        {
            const auto hw = std::thread::hardware_concurrency();
            config_.initial_workers = hw > 0 ? hw : 4;
        }
        if (config_.max_workers == 0)
        {
            config_.max_workers = config_.initial_workers * 8;
        }
        if (config_.min_workers == 0)
        {
            config_.min_workers = 1;
        }

        stop_.store(false, std::memory_order_release);

        StartWorkers(config_.initial_workers);

        if (config_.enable_auto_scale)
        {
            StartScalingLoop();
        }

        LOG_INFO("ThreadPool initialized with {} workers, queue capacity {}, priority queues: High/Medium/Low",
                 config_.initial_workers, config_.initial_queue_capacity);
    }

    /**
     * @brief 启动指定数量的工作线程
     *
     * 创建 count 个工作线程，每个线程执行 WorkerLoop()。
     *
     * @param count 要启动的线程数量
     */
    void ThreadPool::StartWorkers(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            workers_.emplace_back(&ThreadPool::WorkerLoop, this);
        }
    }

    /**
     * @brief 工作线程主循环
     *
     * 每个工作线程执行此循环，不断从队列中获取任务并执行。
     * 任务获取遵循严格的优先级顺序：High -> Medium -> Low。
     *
     * 流程：
     * 1. 等待队列有任务或停止信号
     * 2. 按优先级顺序从队列取任务
     * 3. 执行任务并更新统计指标
     *
     * @note 使用条件变量实现高效等待，避免忙等待
     */
    void ThreadPool::WorkerLoop()
    {
        while (!stop_.load(std::memory_order_acquire))
        {
            TaskWrapper wrapper;
            bool got_task = false;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);

                idle_workers_.fetch_add(1, std::memory_order_relaxed);

                queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                                   [this]()
                                   {
                                       return stop_.load(std::memory_order_acquire) ||
                                           !high_queue_.empty() ||
                                           !medium_queue_.empty() ||
                                           !low_queue_.empty();
                                   });

                idle_workers_.fetch_sub(1, std::memory_order_relaxed);

                if (stop_.load(std::memory_order_acquire))
                {
                    break;
                }

                if (!high_queue_.empty())
                {
                    wrapper = std::move(high_queue_.front());
                    high_queue_.pop();
                    got_task = true;
                }
                else if (!medium_queue_.empty())
                {
                    wrapper = std::move(medium_queue_.front());
                    medium_queue_.pop();
                    got_task = true;
                }
                else if (!low_queue_.empty())
                {
                    wrapper = std::move(low_queue_.front());
                    low_queue_.pop();
                    got_task = true;
                }

                if (got_task)
                {
                    queue_cv_.notify_one();
                }
            }

            if (!got_task)
            {
                continue;
            }

            queue_size_.fetch_sub(1, std::memory_order_relaxed);
            ExecuteTask(wrapper);
        }
    }

    /**
     * @brief 执行单个任务
     *
     * 执行任务函数，捕获所有异常并调用异常处理器。
     * 更新活跃任务计数和延迟统计。
     *
     * @param wrapper 任务包装器，包含任务函数和元数据
     */
    void ThreadPool::ExecuteTask(TaskWrapper& wrapper)
    {
        active_tasks_.fetch_add(1, std::memory_order_relaxed);
        UpdatePeakMetrics();

        try
        {
            wrapper.task();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Task threw unhandled exception: {}", e.what());
            if (panic_handler_)
            {
                panic_handler_(e.what());
            }
        }
        catch (...)
        {
            LOG_ERROR("Task threw unknown unhandled exception");
            if (panic_handler_)
            {
                panic_handler_("unknown exception");
            }
        }

        active_tasks_.fetch_sub(1, std::memory_order_relaxed);
        RecordTaskCompleted(wrapper);
    }

    /**
     * @brief 启动自动扩缩容检查线程
     *
     * 创建一个后台线程，定期检查线程池负载并决定是否扩容。
     * 缩容逻辑目前仅记录日志，实际缩容需要更复杂的实现。
     */
    void ThreadPool::StartScalingLoop()
    {
        scale_thread_ = std::thread(
            [this]()
            {
                while (!stop_.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(config_.scale_interval);
                    if (stop_.load(std::memory_order_acquire))
                    {
                        break;
                    }
                    CheckAndScale();
                }
            });
    }

    /**
     * @brief 检查并执行扩缩容
     *
     * 根据当前负载因子和队列占用率决定是否扩容。
     * 扩容条件：负载因子 >= scale_threshold 或 队列占用率 >= scale_threshold
     * 且当前线程数 < 最大线程数，且已过冷却时间。
     */
    void ThreadPool::CheckAndScale()
    {
        if (!config_.enable_auto_scale)
        {
            return;
        }

        const auto current_workers = workers_.size();
        const auto current_active = static_cast<size_t>(active_tasks_.load(std::memory_order_relaxed));
        const auto current_queue = static_cast<size_t>(queue_size_.load(std::memory_order_relaxed));

        double load_factor = 0.0;
        if (current_workers > 0)
        {
            load_factor = static_cast<double>(current_active) / static_cast<double>(current_workers);
        }

        const double queue_factor = static_cast<double>(current_queue) / static_cast<double>(config_.
            initial_queue_capacity);

        const auto now = std::chrono::steady_clock::now();

        if ((load_factor >= config_.scale_threshold || queue_factor >= config_.scale_threshold) &&
            current_workers < config_.max_workers)
        {
            const auto since_last_scale = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scale_up_);
            if (since_last_scale >= config_.scale_up_cooldown)
            {
                const size_t to_add = std::min(config_.scale_up_step, config_.max_workers - current_workers);
                if (to_add > 0)
                {
                    StartWorkers(to_add);
                    last_scale_up_ = now;
                    {
                        std::lock_guard<std::mutex> lock(metrics_mutex_);
                        ++metrics_.scale_up_count;
                    }
                    LOG_INFO("ThreadPool scaling up: {} -> {} workers", current_workers, current_workers + to_add);
                }
            }
        }
        else if (load_factor <= config_.shrink_threshold && queue_factor <= config_.shrink_threshold &&
            current_workers > config_.min_workers)
        {
            const auto since_last_scale = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scale_down_);
            if (since_last_scale >= config_.scale_down_cooldown)
            {
                LOG_DEBUG("ThreadPool considering scale down: {} workers, load {:.2f}", current_workers, load_factor);
            }
        }
    }

    /**
     * @brief 停止线程池
     *
     * 执行优雅停止流程：
     * 1. 设置停止标志
     * 2. 唤醒所有等待的工作线程
     * 3. 等待扩缩容线程结束
     * 4. 可选：等待队列中的任务执行完成
     * 5. 清空队列
     * 6. 等待所有工作线程结束
     *
     * @param wait_for_tasks 是否等待队列中的任务执行完成
     */
    void ThreadPool::Stop(bool wait_for_tasks)
    {
        if (stop_.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        LOG_INFO("ThreadPool stopping, wait_for_tasks={}", wait_for_tasks);

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_cv_.notify_all();
        }

        if (scale_thread_.joinable())
        {
            scale_thread_.join();
        }

        if (wait_for_tasks)
        {
            // Use condition variable instead of spin-wait. Workers notify queue_cv_
            // after completing each task, so we wake promptly when the queue drains.
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(30), [this]()
            {
                return queue_size_.load(std::memory_order_relaxed) == 0;
            }))
            {
                LOG_WARN("ThreadPool stop timed out waiting for queue drain, remaining={}",
                         queue_size_.load(std::memory_order_relaxed));
            }
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            while (!high_queue_.empty())
            {
                high_queue_.pop();
                queue_size_.fetch_sub(1, std::memory_order_relaxed);
            }
            while (!medium_queue_.empty())
            {
                medium_queue_.pop();
                queue_size_.fetch_sub(1, std::memory_order_relaxed);
            }
            while (!low_queue_.empty())
            {
                low_queue_.pop();
                queue_size_.fetch_sub(1, std::memory_order_relaxed);
            }
        }

        queue_cv_.notify_all();

        for (auto& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        workers_.clear();

        LOG_INFO("ThreadPool stopped, metrics: submitted={}, completed={}, dropped={}", metrics_.total_tasks_submitted,
                 metrics_.total_tasks_completed, metrics_.total_tasks_dropped);
    }

    /**
     * @brief 获取指定优先级对应的队列引用
     *
     * @param priority 任务优先级
     * @return std::queue<TaskWrapper>& 对应队列的引用
     */
    std::queue<ThreadPool::TaskWrapper>& ThreadPool::GetQueue(TaskPriority priority)
    {
        switch (priority)
        {
        case TaskPriority::High:
            return high_queue_;
        case TaskPriority::Medium:
            return medium_queue_;
        case TaskPriority::Low:
            return low_queue_;
        default:
            return medium_queue_;
        }
    }

    /**
     * @brief 获取指定优先级对应的队列常量引用
     *
     * @param priority 任务优先级
     * @return const std::queue<TaskWrapper>& 对应队列的常量引用
     */
    const std::queue<ThreadPool::TaskWrapper>& ThreadPool::GetQueue(TaskPriority priority) const
    {
        switch (priority)
        {
        case TaskPriority::High:
            return high_queue_;
        case TaskPriority::Medium:
            return medium_queue_;
        case TaskPriority::Low:
            return low_queue_;
        default:
            return medium_queue_;
        }
    }

    /**
     * @brief 计算三个队列的总大小
     *
     * @return int32_t 高/中/低三个队列的任务总数
     * @note 调用者必须持有 queue_mutex_
     */
    int32_t ThreadPool::TotalQueueSize() const
    {
        return static_cast<int32_t>(high_queue_.size() + medium_queue_.size() + low_queue_.size());
    }

    /**
     * @brief 获取指定优先级队列的大小
     *
     * 线程安全地获取指定优先级队列中的任务数量。
     *
     * @param priority 任务优先级
     * @return size_t 该优先级队列中的任务数
     */
    size_t ThreadPool::QueueSizeByPriority(TaskPriority priority) const
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return GetQueue(priority).size();
    }

    /**
     * @brief 根据名称字符串获取优先级枚举值
     *
     * 支持大小写不敏感的名称匹配。
     *
     * @param priority_name 优先级名称字符串
     * @return TaskPriority 对应的优先级枚举值
     */
    TaskPriority ThreadPool::GetTaskPriority(const std::string& priority_name) const
    {
        if (priority_name == "high" || priority_name == "High" || priority_name == "HIGH")
        {
            return TaskPriority::High;
        }
        if (priority_name == "low" || priority_name == "Low" || priority_name == "LOW")
        {
            return TaskPriority::Low;
        }
        return TaskPriority::Medium;
    }

    /**
     * @brief 获取线程池运行指标
     *
     * 线程安全地获取当前线程池的运行指标快照。
     *
     * @return ThreadPoolMetrics 指标快照
     */
    ThreadPoolMetrics ThreadPool::GetMetrics() const
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        auto m = metrics_;
        m.current_queue_size = queue_size_.load(std::memory_order_relaxed);
        m.current_active_workers = active_tasks_.load(std::memory_order_relaxed);
        m.current_idle_workers = idle_workers_.load(std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> qlock(queue_mutex_);
            m.high_queue_size = static_cast<int32_t>(high_queue_.size());
            m.medium_queue_size = static_cast<int32_t>(medium_queue_.size());
            m.low_queue_size = static_cast<int32_t>(low_queue_.size());
        }

        return m;
    }

    /**
     * @brief 记录任务提交
     *
     * 增加队列大小计数和提交任务计数。
     */
    void ThreadPool::RecordTaskSubmitted()
    {
        queue_size_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.total_tasks_submitted;
    }

    /**
     * @brief 记录任务完成并更新延迟统计
     *
     * 计算任务从入队到完成的延迟，并更新平均延迟。
     *
     * @param wrapper 任务包装器，包含入队时间
     */
    void ThreadPool::RecordTaskCompleted(const TaskWrapper& wrapper)
    {
        const auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - wrapper.enqueue_time)
            .count();

        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.total_tasks_completed;

        const auto total = metrics_.total_tasks_completed;
        const auto avg = metrics_.average_task_latency_ns;
        metrics_.average_task_latency_ns = (avg * (total - 1) + static_cast<uint64_t>(latency)) / total;
    }

    /**
     * @brief 记录任务丢弃
     *
     * 当任务因线程池停止而被拒绝时调用。
     */
    void ThreadPool::RecordTaskDropped()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.total_tasks_dropped;
    }

    /**
     * @brief 记录任务超时
     *
     * 当任务因超时而被拒绝时调用。
     */
    void ThreadPool::RecordTaskTimeout()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.total_tasks_timeout;
    }

    /**
     * @brief 记录队列满
     *
     * 当任务因队列满而被拒绝时调用。
     */
    void ThreadPool::RecordQueueFull()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.queue_full_reject_count;
        ++metrics_.total_tasks_dropped;
    }

    /**
     * @brief 更新峰值指标
     *
     * 比较并更新活跃线程数和队列大小的峰值记录。
     */
    void ThreadPool::UpdatePeakMetrics()
    {
        const auto active = active_tasks_.load(std::memory_order_relaxed);
        const auto queue_sz = queue_size_.load(std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(metrics_mutex_);
        if (active > metrics_.peak_active_workers)
        {
            metrics_.peak_active_workers = active;
        }
        if (queue_sz > metrics_.peak_queue_size)
        {
            metrics_.peak_queue_size = queue_sz;
        }
    }

    /**
     * @brief 获取全局线程池单例
     *
     * 返回进程级别的线程池单例实例。
     * 使用静态局部变量实现线程安全的单例模式。
     *
     * @return ThreadPool& 全局线程池引用
     */
    ThreadPool& GetGlobalThreadPool()
    {
        static ThreadPool instance;
        return instance;
    }
} // namespace tyke