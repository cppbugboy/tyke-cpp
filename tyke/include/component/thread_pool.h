/**
 * @file thread_pool.h
 * @brief 高性能优先级线程池（C++17），支持高/中/低三级任务优先级调度。
 *
 * 特性：
 * - 三分离优先级队列（High/Medium/Low），严格按优先级调度
 * - 高优先级任务始终优先于中/低优先级任务获取线程资源
 * - 使用互斥锁+条件变量保证线程安全的优先级调度
 * - 支持动态线程数调整
 * - 支持优雅降级策略
 * - 完善的指标统计，含各优先级队列状态监控
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
    enum class TaskPriority : int
    {
        Low = 0,
        Medium = 1,
        High = 2
    };

    struct ThreadPoolMetrics
    {
        uint64_t total_tasks_submitted = 0;
        uint64_t total_tasks_completed = 0;
        uint64_t total_tasks_dropped = 0;
        uint64_t total_tasks_timeout = 0;
        int32_t current_queue_size = 0;
        int32_t high_queue_size = 0;
        int32_t medium_queue_size = 0;
        int32_t low_queue_size = 0;
        int32_t current_active_workers = 0;
        int32_t current_idle_workers = 0;
        int32_t peak_queue_size = 0;
        int32_t peak_active_workers = 0;
        uint64_t average_task_latency_ns = 0;
        uint64_t queue_full_reject_count = 0;
        uint64_t scale_up_count = 0;
        uint64_t scale_down_count = 0;
    };

    struct ThreadPoolConfig
    {
        size_t initial_workers = 0;
        size_t initial_queue_capacity = 4096;
        size_t max_workers = 0;
        size_t min_workers = 1;
        bool enable_auto_scale = true;
        double scale_threshold = 0.8;
        double shrink_threshold = 0.2;
        std::chrono::milliseconds scale_interval{5000};
        std::chrono::milliseconds scale_up_cooldown{2000};
        std::chrono::milliseconds scale_down_cooldown{10000};
        size_t scale_up_step = 2;
        size_t scale_down_step = 1;
        bool enable_metrics = true;
        std::chrono::milliseconds task_timeout{30000};
    };

    inline ThreadPoolConfig DefaultThreadPoolConfig()
    {
        ThreadPoolConfig config;
        const auto hw_threads = std::thread::hardware_concurrency();
        config.initial_workers = hw_threads > 0 ? hw_threads : 4;
        config.max_workers = config.initial_workers * 8;
        config.min_workers = config.initial_workers;
        return config;
    }

    class ThreadPool
    {
    public:
        explicit ThreadPool();
        ~ThreadPool();

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        void Init(size_t threads);
        void InitWithConfig(const ThreadPoolConfig &config);
        void Stop(bool wait_for_tasks = true);

        template <class F, class... Args>
        auto Enqueue(F &&f, Args &&...args) -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
        {
            return EnqueueWithPriority(TaskPriority::Medium, std::forward<F>(f), std::forward<Args>(args)...);
        }

        template <class F, class... Args>
        auto EnqueueWithPriority(TaskPriority priority, F &&f, Args &&...args)
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

        template <class F, class... Args>
        auto EnqueueWithTimeout(std::chrono::milliseconds timeout, F &&f, Args &&...args)
            -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
        {
            auto result = Enqueue(std::forward<F>(f), std::forward<Args>(args)...);
            if (!result)
            {
                RecordTaskTimeout();
            }
            return result;
        }

        template <class F, class... Args>
        auto EnqueueOrExecute(F &&f, Args &&...args) -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
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

        size_t WorkerCount() const { return workers_.size(); }
        size_t ActiveTaskCount() const { return static_cast<size_t>(active_tasks_.load()); }
        size_t QueueSize() const { return static_cast<size_t>(queue_size_.load()); }
        size_t QueueSizeByPriority(TaskPriority priority) const;
        bool IsRunning() const { return !stop_.load(std::memory_order_acquire); }
        ThreadPoolMetrics GetMetrics() const;
        TaskPriority GetTaskPriority(const std::string &priority_name) const;

        void SetPanicHandler(std::function<void(const std::string &)> handler) { panic_handler_ = std::move(handler); }

    private:
        struct TaskWrapper
        {
            std::function<void()> task;
            std::chrono::steady_clock::time_point enqueue_time;
            TaskPriority priority = TaskPriority::Medium;
        };

        void StartWorkers(size_t count);
        void WorkerLoop();
        void ExecuteTask(TaskWrapper &wrapper);
        void StartScalingLoop();
        void CheckAndScale();
        void RecordTaskSubmitted();
        void RecordTaskCompleted(const TaskWrapper &wrapper);
        void RecordTaskDropped();
        void RecordTaskTimeout();
        void RecordQueueFull();
        void UpdatePeakMetrics();

        std::queue<TaskWrapper> &GetQueue(TaskPriority priority);
        const std::queue<TaskWrapper> &GetQueue(TaskPriority priority) const;
        int32_t TotalQueueSize() const;

        std::queue<TaskWrapper> high_queue_;
        std::queue<TaskWrapper> medium_queue_;
        std::queue<TaskWrapper> low_queue_;
        mutable std::mutex queue_mutex_;
        std::condition_variable queue_cv_;

        std::vector<std::thread> workers_;
        std::atomic<bool> stop_{false};
        std::atomic<int32_t> active_tasks_{0};
        std::atomic<int32_t> idle_workers_{0};
        std::atomic<int32_t> queue_size_{0};

        ThreadPoolConfig config_;
        std::thread scale_thread_;
        std::chrono::steady_clock::time_point last_scale_up_;
        std::chrono::steady_clock::time_point last_scale_down_;

        mutable std::mutex metrics_mutex_;
        ThreadPoolMetrics metrics_;

        std::function<void(const std::string &)> panic_handler_;
    };

    ThreadPool &GetGlobalThreadPool();

} // namespace tyke
