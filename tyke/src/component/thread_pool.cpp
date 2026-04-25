#include "component/thread_pool.h"

namespace tyke
{
    ThreadPool::ThreadPool() : stop_(false)
    {
    }

    ThreadPool::~ThreadPool()
    {
        Stop(true);
    }

    void ThreadPool::Init(size_t threads)
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (!workers_.empty()) return; // 已初始化
        if (threads == 0) return;

        stop_.store(false, std::memory_order_release);
        for (size_t i = 0; i < threads; ++i)
        {
            workers_.emplace_back([this]
            {
                for (;;)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock2(queue_mutex_);
                        condition_.wait(lock2, [this]
                        {
                            return stop_.load(std::memory_order_acquire) || !tasks_.empty();
                        });
                        if (stop_.load(std::memory_order_acquire) && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    void ThreadPool::Stop(bool wait_for_tasks)
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_.exchange(true, std::memory_order_acq_rel)) return;
            if (!wait_for_tasks)
            {
                std::queue<std::function<void()>> empty;
                std::swap(tasks_, empty);
            }
        }
        condition_.notify_all();
        for (auto& worker : workers_)
        {
            if (worker.joinable()) worker.join();
        }
        workers_.clear();
    }

    ThreadPool& GetGlobalThreadPool()
    {
        static ThreadPool instance;
        return instance;
    }
} // namespace tyke
