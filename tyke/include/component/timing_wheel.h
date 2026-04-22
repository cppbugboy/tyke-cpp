/**
 * @file timing_wheel.h
 * @brief 工业级多级时间轮 (Hierarchical Timing Wheel)
 * @author Standardized C++ Implementation
 */

#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <memory>
#include <atomic>

namespace tyke
{
    // 定时器唯一 ID
    using TimerId = uint64_t;
    using TimePoint = std::chrono::steady_clock::time_point;

    struct TimerTask
    {
        TimerId id{};
        std::function<void()> callback;
        TimePoint expire_time;
        bool cancelled = false;
    };

    struct WheelLevel
    {
        uint64_t tick_interval_ms{};  // 当前层级一个 tick 的跨度
        uint32_t slot_count{};        // 当前层级的槽位数
        uint32_t current_index = 0; // 当前拨盘指针
        std::vector<std::list<std::shared_ptr<TimerTask>>> slots;
    };

    class TimingWheel
    {
    public:
        /**
         * @brief 构造多级时间轮
         * @param base_tick_ms 基础精度（例如 10ms）
         * @param slots_per_level 每一层的槽位数。
         * 示例: base=10, slots={256, 64, 64}
         * - L0: 跨度 10ms * 256 = 2.56秒
         * - L1: 跨度 2.56s * 64 = 163.8秒
         * - L2: 跨度 163.8s * 64 = 约2.9小时
         */
        TimingWheel(uint32_t base_tick_ms = 10, const std::vector<uint32_t>& slots_per_level = {256, 64, 64, 64})
            : base_tick_ms_(base_tick_ms), stop_(false), next_id_(0)
        {
            uint64_t current_tick_ms = base_tick_ms;
            for (uint32_t slots : slots_per_level)
            {
                WheelLevel level;
                level.tick_interval_ms = current_tick_ms;
                level.slot_count = slots;
                level.slots.resize(slots);
                levels_.push_back(std::move(level));

                current_tick_ms *= slots; // 下一层的 tick 跨度是当前层的总跨度
            }

            last_tick_time_ = std::chrono::steady_clock::now();
            worker_thread_ = std::thread(&TimingWheel::WorkerLoop, this);
        }

        ~TimingWheel()
        {
            Stop();
        }

        // 禁止拷贝
        TimingWheel(const TimingWheel&) = delete;
        TimingWheel& operator=(const TimingWheel&) = delete;

        /**
         * @brief 添加定时任务
         * @param delay_ms 延迟毫秒数
         * @param cb 到期执行的回调函数
         * @return TimerId 用于取消任务的唯一句柄
         */
        TimerId AddTask(uint32_t delay_ms, std::function<void()> cb)
        {
            if (!cb) return 0;

            std::lock_guard<std::mutex> lock(mutex_);
            TimerId id = ++next_id_;

            auto task = std::make_shared<TimerTask>();
            task->id = id;
            task->callback = std::move(cb);
            task->expire_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);

            bool was_empty = task_map_.empty();
            task_map_[id] = task;

            // 如果之前为空，重置起始时间并唤醒休眠的线程
            if (was_empty)
            {
                last_tick_time_ = std::chrono::steady_clock::now();
                cv_.notify_one();
            }

            InsertTask(task);
            return id;
        }

        /**
         * @brief 取消定时任务 (O(1) 复杂度)
         */
        void CancelTask(TimerId id)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = task_map_.find(id);
            if (it != task_map_.end())
            {
                it->second->cancelled = true; // 懒删除标记
                task_map_.erase(it);
            }
        }

        /**
         * @brief 停止时间轮
         */
        void Stop()
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stop_) return;
                stop_ = true;
                cv_.notify_all();
            }
            if (worker_thread_.joinable())
            {
                worker_thread_.join();
            }
        }

    private:
        void InsertTask(std::shared_ptr<TimerTask> task)
        {
            if (task->cancelled) return;

            // 计算任务离时间轮当前指针的距离
            int64_t distance_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                task->expire_time - last_tick_time_).count();

            // 如果已经过期，直接放入待执行队列
            if (distance_ms <= 0)
            {
                expired_tasks_.push_back(task);
                return;
            }

            // 寻找应该插入的层级
            for (size_t i = 0; i < levels_.size(); ++i)
            {
                auto& level = levels_[i];
                uint64_t max_capacity_ms = level.tick_interval_ms * level.slot_count;

                // 如果距离落在当前层级的最大跨度内，或者是最后一层（兜底）
                if (static_cast<uint64_t>(distance_ms) < max_capacity_ms || i == levels_.size() - 1)
                {
                    uint64_t ticks = distance_ms / level.tick_interval_ms;
                    uint32_t slot = (level.current_index + ticks) % level.slot_count;
                    level.slots[slot].push_back(std::move(task));
                    return;
                }
            }
        }

        void Cascade(size_t level_idx)
        {
            auto& level = levels_[level_idx];
            level.current_index = (level.current_index + 1) % level.slot_count;

            // 取出当前推进到的槽位中的所有任务
            auto tasks = std::move(level.slots[level.current_index]);
            level.slots[level.current_index].clear();

            for (auto& task : tasks)
            {
                if (task->cancelled) continue;

                if (level_idx == 0)
                {
                    // 最底层的任务说明精准到期了
                    expired_tasks_.push_back(task);
                }
                else
                {
                    // 高层任务临近，重新计算并插入到底层（即层级降级）
                    InsertTask(task);
                }
            }

            // 如果当前层转满了一圈（回到了0），触发上一层转动一格
            if (level.current_index == 0 && level_idx + 1 < levels_.size())
            {
                Cascade(level_idx + 1);
            }
        }

        void WorkerLoop()
        {
            std::unique_lock<std::mutex> lock(mutex_);

            while (!stop_)
            {
                // 优化：如果没有任务，进入休眠直到有新任务加入，节省 CPU
                if (task_map_.empty() && expired_tasks_.empty())
                {
                    cv_.wait(lock, [this]() {
                        return stop_ || !task_map_.empty() || !expired_tasks_.empty();
                    });
                    if (stop_) break;
                }

                auto now = std::chrono::steady_clock::now();

                // 追赶丢失的 Tick，防止机器休眠或高负载引起的漂移
                while (last_tick_time_ + std::chrono::milliseconds(base_tick_ms_) <= now)
                {
                    last_tick_time_ += std::chrono::milliseconds(base_tick_ms_);
                    Cascade(0); // 驱动最底层齿轮
                }

                // 处理过期任务
                if (!expired_tasks_.empty())
                {
                    std::vector<std::shared_ptr<TimerTask>> tasks_to_run;
                    tasks_to_run.swap(expired_tasks_);

                    // 从 map 中移除记录
                    for (const auto& task : tasks_to_run)
                    {
                        task_map_.erase(task->id);
                    }

                    // 【核心安全设计】：释放锁再去执行回调，坚决避免死锁！
                    lock.unlock();
                    for (const auto& task : tasks_to_run)
                    {
                        if (!task->cancelled && task->callback)
                        {
                            task->callback();
                        }
                    }
                    lock.lock();
                }

                if (stop_) break;

                // 精确等待到下一个 Tick 时间点
                if (!task_map_.empty())
                {
                    auto next_tick_time = last_tick_time_ + std::chrono::milliseconds(base_tick_ms_);
                    cv_.wait_until(lock, next_tick_time, [this]() { return stop_.load(); });
                }
            }
        }

    private:
        uint32_t base_tick_ms_;
        std::atomic<bool> stop_;
        std::atomic<TimerId> next_id_;

        std::vector<WheelLevel> levels_;
        TimePoint last_tick_time_;

        std::thread worker_thread_;
        std::mutex mutex_;
        std::condition_variable cv_;

        // 快速查找用于取消，兼具保活功能
        std::unordered_map<TimerId, std::shared_ptr<TimerTask>> task_map_;
        std::vector<std::shared_ptr<TimerTask>> expired_tasks_;
    };
}