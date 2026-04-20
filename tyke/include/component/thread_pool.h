/**
 * @file thread_pool.h
 * @brief 线程池类 (C++17)
 * @author Nick
 * @date 2026/04/17
 *
 * ThreadPool是线程安全的线程池类，支持任务队列和异步任务执行。
 * 继承自Singleton<ThreadPool>，全局只有一个实例。
 *
 * C++17特性:
 * - 使用std::optional替代nonstd::optional作为Enqueue返回值
 * - 使用std::invoke_result_t替代已废弃的std::result_of
 * - 使用lambda + std::apply替代std::bind进行参数绑定
 *
 * 使用示例：
 * @code
 *   THREAD_POOL_INSTANCE->Init(4);
 *   auto future = THREAD_POOL_INSTANCE->Enqueue([]() { return 42; });
 *   if (future) {
 *       int result = future->get();
 *   }
 * @endcode
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

#include "singleton.h"

namespace tyke
{
/// 线程池单例访问宏
#define THREAD_POOL_INSTANCE ThreadPool::GetInstance()

    /**
     * @brief 线程池类
     *
     * 线程安全的线程池，支持任务队列和异步任务执行。
     * 支持优雅退出和强制退出两种模式。
     */
    class ThreadPool : public Singleton<ThreadPool>
    {
        friend class Singleton<ThreadPool>;

    public:
        /**
         * @brief 初始化线程池
         * @param threads 线程数量
         *
         * 创建指定数量的工作线程。
         */
        void Init(const size_t threads)
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 避免重复初始化导致线程泄漏
            if (!workers_.empty())
                return;

            stop_.store(false, std::memory_order_release);

            for (size_t i = 0; i < threads; ++i)
            {
                workers_.emplace_back(
                    [this]
                    {
                        for (;;)
                        {
                            std::function<void()> task;
                            {
                                std::unique_lock<std::mutex> lock2(this->queue_mutex_);
                                this->condition_.wait(lock2, [this]
                                {
                                    return this->stop_.load(std::memory_order_acquire) || !this->tasks_.empty();
                                });

                                // 线程退出条件：已触发停止 且 任务队列为空
                                if (this->stop_.load(std::memory_order_acquire) && this->tasks_.empty())
                                {
                                    return;
                                }

                                task = std::move(this->tasks_.front());
                                this->tasks_.pop();
                            }

                            // 在锁的范围外执行任务，避免阻塞其他线程取任务
                            task();
                        }
                    }
                );
            }
        }

        /**
         * @brief 停止线程池
         * @param waitForTasks 是否等待队列中任务执行完（默认true）
         *
         * true=优雅退出(等待队列中任务执行完), false=强制退出(丢弃未执行任务)
         */
        void Stop(const bool waitForTasks = true)
        {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                // C++11 支持 exchange，保证只触发一次停止逻辑
                if (stop_.exchange(true, std::memory_order_acq_rel))
                {
                    return; // 已经停止，直接返回
                }

                if (!waitForTasks)
                {
                    // 强制退出：通过交换空队列来清空现有任务
                    std::queue<std::function<void()>> empty_queue;
                    std::swap(tasks_, empty_queue);
                }
            }

            // 唤醒所有正在 wait 的空闲线程，让它们检查 stop_ 标志并退出
            condition_.notify_all();

            for (std::thread& worker : workers_)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }

            // 清空 worker 集合，允许单例线程池在未来被重新 Init()
            workers_.clear();
        }

        /**
         * @brief 将任务加入队列
         * @tparam F 任务函数类型
         * @tparam Args 任务参数类型
         * @param f 任务函数
         * @param args 任务参数
         * @return 成功返回future对象，失败返回nullopt
         */
        template<class F, class... Args>
        auto Enqueue(F&& f, Args&&... args)
            -> std::optional<std::future<std::invoke_result_t<F, Args...>>>
        {
            using return_type = std::invoke_result_t<F, Args...>;

            if (stop_.load(std::memory_order_acquire))
            {
                return std::nullopt;
            }

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                [f = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable
                -> return_type { return std::apply(f, std::move(tup)); }
            );

            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);

                if (stop_.load(std::memory_order_acquire))
                {
                    return std::nullopt;
                }

                tasks_.emplace([task]() { (*task)(); });
            }

            condition_.notify_one();
            return res;
        }

    private:
        /**
         * @brief 构造函数
         */
        ThreadPool() : stop_(false)
        {
        }

        /**
         * @brief 析构函数
         *
         * 析构时默认采取优雅退出策略，保障数据不丢失。
         */
        ~ThreadPool() override
        {
            Stop(true);
        }

        std::vector<std::thread> workers_;              ///< 工作线程集合
        std::queue<std::function<void()>> tasks_;       ///< 任务队列
        std::mutex queue_mutex_;                        ///< 队列互斥锁
        std::condition_variable condition_;             ///< 条件变量
        std::atomic<bool> stop_;                        ///< 停止标志
    };
}
