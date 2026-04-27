/**
 * @file timing_wheel.cpp
 * @brief 工业级多级层次时间轮实现。
 *
 * 本文件实现了 TimingWheel 类的所有成员函数，包括：
 * - 时间轮的初始化与停止
 * - 定时器的添加、取消和查询
 * - 层级级联机制
 * - 驱动线程的主循环
 *
 * @see timing_wheel.h
 * @author Nick
 * @date 2026/04/26
 */

#include "tyke/component/timing_wheel.h"

#include "tyke/component/thread_pool.h" // 用于 GetGlobalThreadPool

namespace tyke
{
    /**
     * @brief 初始化时间轮
     *
     * 根据指定的基础刻度和每层槽位数初始化多级时间轮。
     * 创建驱动线程开始运行。
     *
     * @param base_tick_ms 基础刻度（毫秒）
     * @param slots_per_level 每层槽位数组
     * @return true 初始化成功
     * @return false 初始化失败（参数无效或已初始化）
     */
    bool TimingWheel::Init(uint32_t base_tick_ms, const std::vector<uint32_t>& slots_per_level)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_.load(std::memory_order_acquire))
            return false;
        if (base_tick_ms == 0 || slots_per_level.empty())
            return false;

        base_tick_ms_ = base_tick_ms;
        levels_.clear();
        uint64_t current_tick = base_tick_ms;
        for (uint32_t slot : slots_per_level)
        {
            if (slot == 0)
                return false;
            WheelLevel level;
            level.tick_interval_ms = current_tick;
            level.slot_count = slot;
            level.current_index = 0;
            level.slots.resize(slot);
            levels_.push_back(std::move(level));
            current_tick *= slot;
        }

        last_tick_time_ = std::chrono::steady_clock::now();
        stop_.store(false, std::memory_order_release);
        initialized_.store(true, std::memory_order_release);
        worker_thread_ = std::thread(&TimingWheel::WorkerLoop, this);
        return true;
    }

    /**
     * @brief 添加相对延迟任务
     *
     * @param delay_ms 延迟时间（毫秒）
     * @param cb 到期回调函数
     * @return TimerId 定时器ID，失败返回kInvalidTimerId
     */
    TimerId TimingWheel::AddTask(uint32_t delay_ms, std::function<void()> cb)
    {
        if (!cb)
            return kInvalidTimerId;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_.load(std::memory_order_acquire) || stop_.load(std::memory_order_acquire))
            return kInvalidTimerId;
        auto expire = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
        return InsertNewTask(expire, 0, false, std::move(cb));
    }

    /**
     * @brief 添加绝对截止时间任务
     *
     * @param deadline 绝对截止时间点
     * @param cb 到期回调函数
     * @return TimerId 定时器ID，失败返回kInvalidTimerId
     */
    TimerId TimingWheel::AddTaskAt(TimePoint deadline, std::function<void()> cb)
    {
        if (!cb)
            return kInvalidTimerId;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_.load(std::memory_order_acquire) || stop_.load(std::memory_order_acquire))
            return kInvalidTimerId;
        return InsertNewTask(deadline, 0, false, std::move(cb));
    }

    /**
     * @brief 添加周期性任务
     *
     * @param initial_delay_ms 首次执行前的延迟（毫秒）
     * @param interval_ms 执行间隔（毫秒）
     * @param cb 回调函数
     * @return TimerId 定时器ID，失败返回kInvalidTimerId
     */
    TimerId TimingWheel::AddRepeatedTask(uint32_t initial_delay_ms, uint32_t interval_ms, std::function<void()> cb)
    {
        if (!cb || interval_ms == 0)
            return kInvalidTimerId;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_.load(std::memory_order_acquire) || stop_.load(std::memory_order_acquire))
            return kInvalidTimerId;
        auto expire = std::chrono::steady_clock::now() + std::chrono::milliseconds(initial_delay_ms);
        return InsertNewTask(expire, interval_ms, true, std::move(cb));
    }

    /**
     * @brief 取消定时器
     *
     * @param id 定时器ID
     * @return true 取消成功
     * @return false 定时器不存在
     */
    bool TimingWheel::CancelTask(TimerId id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = task_map_.find(id);
        if (it == task_map_.end())
            return false;
        it->second->cancelled = true;
        task_map_.erase(it);
        return true;
    }

    /**
     * @brief 检查定时器是否活跃
     *
     * @param id 定时器ID
     * @return true 定时器活跃
     * @return false 定时器不存在
     */
    bool TimingWheel::IsTaskActive(TimerId id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return task_map_.find(id) != task_map_.end();
    }

    /**
     * @brief 获取定时器剩余时间
     *
     * @param id 定时器ID
     * @return std::optional<std::chrono::milliseconds> 剩余时间，不存在返回nullopt
     */
    std::optional<std::chrono::milliseconds> TimingWheel::GetRemainingTime(TimerId id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = task_map_.find(id);
        if (it == task_map_.end())
            return std::nullopt;
        auto now = std::chrono::steady_clock::now();
        auto remaining = it->second->expire_time - now;
        if (remaining.count() <= 0)
            return std::chrono::milliseconds(0);
        return std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
    }

    /**
     * @brief 获取活跃任务数量
     *
     * @return size_t 活跃任务数
     */
    size_t TimingWheel::GetActiveTaskCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return task_map_.size();
    }

    /**
     * @brief 停止时间轮
     *
     * 停止驱动线程，清空所有任务。
     */
    void TimingWheel::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_.load(std::memory_order_acquire))
                return;
            stop_.store(true, std::memory_order_release);
            initialized_.store(false, std::memory_order_release);
            cv_.notify_all();
        }
        if (worker_thread_.joinable())
            worker_thread_.join();
        std::lock_guard<std::mutex> lock(mutex_);
        task_map_.clear();
        expired_tasks_.clear();
        levels_.clear();
    }

    /**
     * @brief 检查时间轮是否运行中
     *
     * @return true 运行中
     * @return false 已停止
     */
    bool TimingWheel::IsRunning() const
    {
        return initialized_.load(std::memory_order_acquire) && !stop_.load(std::memory_order_acquire);
    }

    /**
     * @brief 获取最大容量（毫秒）
     *
     * @return uint64_t 时间轮能调度的最大延迟
     */
    uint64_t TimingWheel::GetMaxCapacityMs() const
    {
        if (levels_.empty())
            return 0;
        uint64_t cap = base_tick_ms_;
        for (auto& l : levels_)
            cap *= l.slot_count;
        return cap;
    }

    /**
     * @brief 插入新任务
     *
     * 创建定时器任务并插入到适当的层级和槽位。
     *
     * @param expire 到期时间
     * @param interval 重复间隔
     * @param repeating 是否重复
     * @param cb 回调函数
     * @return TimerId 定时器ID
     */
    TimerId TimingWheel::InsertNewTask(TimePoint expire, uint32_t interval, bool repeating, std::function<void()> cb)
    {
        auto id = GenerateNextId();
        auto task = std::make_shared<TimerTask>();
        task->id = id;
        task->callback = std::move(cb);
        task->expire_time = expire;
        task->interval_ms = interval;
        task->is_repeating = repeating;
        task->cancelled = false;
        task_map_[id] = task;

        // 如果是第一个任务，更新刻度时间并唤醒驱动线程
        if (task_map_.size() == 1)
        {
            last_tick_time_ = std::chrono::steady_clock::now();
            cv_.notify_one();
        }
        InsertTask(task);
        return id;
    }

    /**
     * @brief 生成下一个定时器ID
     *
     * @return TimerId 新的定时器ID
     */
    TimerId TimingWheel::GenerateNextId()
    {
        TimerId id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
        // 跳过无效ID
        if (id == kInvalidTimerId)
        {
            id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
        }
        return id;
    }

    /**
     * @brief 将任务插入到适当的层级和槽位
     *
     * 根据任务的到期时间计算应该放置的层级和槽位。
     *
     * @param task 定时器任务
     */
    void TimingWheel::InsertTask(const std::shared_ptr<TimerTask>& task)
    {
        if (task->cancelled)
            return;
        int64_t distance =
            std::chrono::duration_cast<std::chrono::milliseconds>(task->expire_time - last_tick_time_).count();
        // 已过期的任务直接放入已到期列表
        if (distance <= 0)
        {
            expired_tasks_.push_back(task);
            return;
        }
        // 遍历层级，找到合适的层级和槽位
        for (size_t i = 0; i < levels_.size(); ++i)
        {
            auto& level = levels_[i];
            uint64_t cap = level.tick_interval_ms * level.slot_count;
            if (static_cast<uint64_t>(distance) < cap || i == levels_.size() - 1)
            {
                uint64_t ticks = distance / level.tick_interval_ms;
                uint32_t slot = (level.current_index + static_cast<uint32_t>(ticks)) % level.slot_count;
                level.slots[slot].push_back(task);
                return;
            }
        }
    }

    /**
     * @brief 层级级联
     *
     * 推进指定层级的当前索引，并将该槽位的任务重新调度。
     * 对于非第0层，任务会被降级到更精确的层级。
     *
     * @param level_idx 层级索引
     */
    void TimingWheel::Cascade(size_t level_idx)
    {
        auto& level = levels_[level_idx];
        level.current_index = (level.current_index + 1) % level.slot_count;
        auto tasks = std::move(level.slots[level.current_index]);
        level.slots[level.current_index].clear();

        for (auto& task : tasks)
        {
            if (task->cancelled)
                continue;
            if (level_idx == 0)
            {
                // 第0层任务到期
                expired_tasks_.push_back(std::move(task));
            }
            else
            {
                // 高层级任务降级
                InsertTask(task);
            }
        }
        // 当当前层级完成一圈时，级联到下一层级
        if (level.current_index == 0 && level_idx + 1 < levels_.size())
        {
            Cascade(level_idx + 1);
        }
    }

    /**
     * @brief 驱动线程主循环
     *
     * 循环推进时间轮，处理到期任务，并调度重复任务。
     * 到期回调被提交到全局线程池执行。
     */
    void TimingWheel::WorkerLoop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!stop_.load(std::memory_order_acquire))
        {
            // 等待任务或停止信号
            if (task_map_.empty() && expired_tasks_.empty())
            {
                cv_.wait(
                    lock, [this]
                    {
                        return stop_.load(std::memory_order_acquire) || !task_map_.empty() || !expired_tasks_.empty();
                    });
                if (stop_.load(std::memory_order_acquire))
                    break;
            }

            // 推进时间轮
            auto now = std::chrono::steady_clock::now();
            auto tick = std::chrono::milliseconds(base_tick_ms_);
            while (last_tick_time_ + tick <= now)
            {
                last_tick_time_ += tick;
                Cascade(0);
            }

            // 处理到期任务
            if (!expired_tasks_.empty())
            {
                std::vector<std::shared_ptr<TimerTask>> to_run;
                to_run.swap(expired_tasks_);
                // 从任务映射中移除非重复任务
                for (auto& t : to_run)
                {
                    if (!t->is_repeating)
                        task_map_.erase(t->id);
                }

                lock.unlock();

                // 执行到期回调并收集重复任务
                std::vector<std::shared_ptr<TimerTask>> repeating;
                for (auto& task : to_run)
                {
                    if (!task->cancelled && task->callback)
                    {
                        auto cb = task->callback;
                        // 提交到全局线程池执行
                        GetGlobalThreadPool().Enqueue(
                            [cb = std::move(cb)]()
                            {
                                try
                                {
                                    cb();
                                }
                                catch (...)
                                {
                                }
                            });
                    }
                    if (task->is_repeating && !task->cancelled)
                    {
                        repeating.push_back(task);
                    }
                }

                lock.lock();

                // 重新调度重复任务
                for (auto& task : repeating)
                {
                    if (task->cancelled)
                    {
                        task_map_.erase(task->id);
                        continue;
                    }
                    task->expire_time += std::chrono::milliseconds(task->interval_ms);
                    InsertTask(task);
                }
            }

            if (stop_.load(std::memory_order_acquire))
                break;

            // 等待下一个刻度或新任务
            if (!task_map_.empty())
            {
                auto next_tick = last_tick_time_ + std::chrono::milliseconds(base_tick_ms_);
                cv_.wait_until(lock, next_tick, [this] { return stop_.load(std::memory_order_acquire); });
            }
            else if (expired_tasks_.empty())
            {
                cv_.wait(
                    lock, [this]
                    {
                        return stop_.load(std::memory_order_acquire) || !task_map_.empty() || !expired_tasks_.empty();
                    });
            }
        }
    }

    /**
     * @brief 获取全局时间轮单例
     *
     * @return TimingWheel& 全局时间轮引用
     */
    TimingWheel& GetGlobalTimingWheel()
    {
        static TimingWheel instance;
        return instance;
    }
} // namespace tyke