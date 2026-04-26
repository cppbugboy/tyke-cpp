/**
 * @file thread_pool.cpp
 * @brief 高性能线程池实现，集成moodycamel::ConcurrentQueue。
 */

#include "component/thread_pool.h"

#include "common/log_def.h"

#include <algorithm>

namespace tyke
{
    ThreadPool::ThreadPool() : queue_(4096)
    {
    }

    ThreadPool::~ThreadPool()
    {
        Stop(true);
    }

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

    void ThreadPool::InitWithConfig(const ThreadPoolConfig &config)
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
        queue_ = moodycamel::BlockingConcurrentQueue<TaskWrapper>(config_.initial_queue_capacity);

        StartWorkers(config_.initial_workers);

        if (config_.enable_auto_scale)
        {
            StartScalingLoop();
        }

        LOG_INFO("ThreadPool initialized with {} workers, queue capacity {}", config_.initial_workers,
                 config_.initial_queue_capacity);
    }

    void ThreadPool::StartWorkers(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            workers_.emplace_back(&ThreadPool::WorkerLoop, this);
        }
    }

    void ThreadPool::WorkerLoop()
    {
        TaskWrapper wrapper;
        while (!stop_.load(std::memory_order_acquire))
        {
            idle_workers_.fetch_add(1, std::memory_order_relaxed);

            bool got = queue_.wait_dequeue_timed(wrapper, std::chrono::milliseconds(100));

            idle_workers_.fetch_sub(1, std::memory_order_relaxed);

            if (!got)
            {
                if (stop_.load(std::memory_order_acquire))
                {
                    break;
                }
                continue;
            }

            queue_size_.fetch_sub(1, std::memory_order_relaxed);
            ExecuteTask(wrapper);
        }
    }

    void ThreadPool::ExecuteTask(TaskWrapper &wrapper)
    {
        active_tasks_.fetch_add(1, std::memory_order_relaxed);
        UpdatePeakMetrics();

        try
        {
            wrapper.task();
        }
        catch (const std::exception &e)
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

        const double queue_factor = static_cast<double>(current_queue) / static_cast<double>(config_.initial_queue_capacity);

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

    void ThreadPool::Stop(bool wait_for_tasks)
    {
        if (stop_.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        LOG_INFO("ThreadPool stopping, wait_for_tasks={}", wait_for_tasks);

        if (scale_thread_.joinable())
        {
            scale_thread_.join();
        }

        if (wait_for_tasks)
        {
            while (queue_size_.load(std::memory_order_relaxed) > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        TaskWrapper dummy;
        while (queue_.try_dequeue(dummy))
        {
            queue_size_.fetch_sub(1, std::memory_order_relaxed);
        }

        for (auto &worker : workers_)
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

    ThreadPoolMetrics ThreadPool::GetMetrics() const
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        auto m = metrics_;
        m.current_queue_size = queue_size_.load(std::memory_order_relaxed);
        m.current_active_workers = active_tasks_.load(std::memory_order_relaxed);
        m.current_idle_workers = idle_workers_.load(std::memory_order_relaxed);
        return m;
    }

    void ThreadPool::RecordTaskSubmitted()
    {
        queue_size_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.total_tasks_submitted;
    }

    void ThreadPool::RecordTaskCompleted(const TaskWrapper &wrapper)
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

    void ThreadPool::RecordTaskDropped()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.total_tasks_dropped;
    }

    void ThreadPool::RecordTaskTimeout()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.total_tasks_timeout;
    }

    void ThreadPool::RecordQueueFull()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.queue_full_reject_count;
        ++metrics_.total_tasks_dropped;
    }

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

    ThreadPool &GetGlobalThreadPool()
    {
        static ThreadPool instance;
        return instance;
    }

} // namespace tyke
