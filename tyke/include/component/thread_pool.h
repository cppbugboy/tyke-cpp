/**
 * @file thread_pool.h
 * @brief 全局线程池（C++17）。
 *
 * 单例风格的固定大小线程池，支持优雅/强制停止，任务提交返回 future。
 */

#pragma once

#include <atomic>
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
    class ThreadPool
    {
    public:
        explicit ThreadPool();
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /**
         * @brief 初始化工作线程。
         * @param threads 线程数量，为 0 则不会启动任何线程。
         */
        void Init(size_t threads);

        /**
         * @brief 停止线程池。
         * @param wait_for_tasks true 等待队列中剩余任务执行完；false 立即丢弃未开始任务。
         */
        void Stop(bool wait_for_tasks = true);

        /**
         * @brief 提交任务，返回 std::future 或 std::nullopt（线程池未初始化/已停止）。
         * @tparam F 可调用对象类型。
         * @tparam Args 参数类型。
         * @param f 可调用对象。
         * @param args 参数。
         * @return 包含 future 的 optional，失败时为空。
         */
        template <class F, class... Args>
        auto Enqueue(F&& f, Args&&... args) -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
        {
            if (workers_.empty())
                return std::nullopt;
            if (stop_.load(std::memory_order_acquire))
                return std::nullopt;

            using return_type = std::invoke_result_t<F, Args...>;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                [f = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> return_type
                {
                    return std::apply(f, std::move(tup));
                });

            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (stop_.load(std::memory_order_relaxed))
                    return std::nullopt;
                tasks_.emplace([task]() { (*task)(); });
            }
            condition_.notify_one();
            return res;
        }

        size_t WorkerCount() const
        {
            return workers_.size();
        }

    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex queue_mutex_;
        std::condition_variable condition_;
        std::atomic<bool> stop_{false};
    };

    /** @brief 全局线程池访问函数，在首次调用前需要调用 Init()。 */
    ThreadPool& GetGlobalThreadPool();
} // namespace tyke