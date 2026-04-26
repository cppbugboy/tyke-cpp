/**
 * @file thread_pool.h
 * @brief 高性能线程池（C++17），集成moodycamel::ConcurrentQueue。
 *
 * 特性：
 * - 使用无锁并发队列，高性能多生产者多消费者
 * - 支持动态线程数调整
 * - 支持优雅降级策略
 * - 完善的指标统计
 */

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>
#include <blockingconcurrentqueue.h>

namespace tyke
{
    struct ThreadPoolMetrics
    {
        uint64_t total_tasks_submitted = 0;
        uint64_t total_tasks_completed = 0;
        uint64_t total_tasks_dropped = 0;
        uint64_t total_tasks_timeout = 0;
        int32_t current_queue_size = 0;
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

            if (!queue_.try_enqueue(wrapper))
            {
                RecordQueueFull();
                return std::nullopt;
            }

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
        bool IsRunning() const { return !stop_.load(std::memory_order_acquire); }
        ThreadPoolMetrics GetMetrics() const;

        void SetPanicHandler(std::function<void(const std::string &)> handler) { panic_handler_ = std::move(handler); }

    private:
        struct TaskWrapper
        {
            std::function<void()> task;
            std::chrono::steady_clock::time_point enqueue_time;
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

        moodycamel::BlockingConcurrentQueue<TaskWrapper> queue_;
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
