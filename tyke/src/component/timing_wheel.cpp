#include "component/timing_wheel.h"

#include "component/thread_pool.h"// for GetGlobalThreadPool in TODO comments

namespace tyke
{
bool TimingWheel::Init(uint32_t base_tick_ms, const std::vector<uint32_t> &slots_per_level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_)
        return false;
    if (base_tick_ms == 0 || slots_per_level.empty())
        return false;

    base_tick_ms_ = base_tick_ms;
    levels_.clear();
    uint64_t current_tick = base_tick_ms;
    for (uint32_t slot: slots_per_level)
    {
        if (slot == 0)
            return false;
        WheelLevel level;
        level.tick_interval_ms = current_tick;
        level.slot_count       = slot;
        level.current_index    = 0;
        level.slots.resize(slot);
        levels_.push_back(std::move(level));
        current_tick *= slot;
    }

    last_tick_time_ = std::chrono::steady_clock::now();
    stop_.store(false, std::memory_order_release);
    initialized_   = true;
    worker_thread_ = std::thread(&TimingWheel::WorkerLoop, this);
    return true;
}

TimerId TimingWheel::AddTask(uint32_t delay_ms, std::function<void()> cb)
{
    if (!cb)
        return kInvalidTimerId;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || stop_.load(std::memory_order_acquire))
        return kInvalidTimerId;
    auto expire = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    return InsertNewTask(expire, 0, false, std::move(cb));
}

TimerId TimingWheel::AddTaskAt(TimePoint deadline, std::function<void()> cb)
{
    if (!cb)
        return kInvalidTimerId;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || stop_.load(std::memory_order_acquire))
        return kInvalidTimerId;
    return InsertNewTask(deadline, 0, false, std::move(cb));
}

TimerId TimingWheel::AddRepeatedTask(uint32_t initial_delay_ms, uint32_t interval_ms, std::function<void()> cb)
{
    if (!cb || interval_ms == 0)
        return kInvalidTimerId;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || stop_.load(std::memory_order_acquire))
        return kInvalidTimerId;
    auto expire = std::chrono::steady_clock::now() + std::chrono::milliseconds(initial_delay_ms);
    return InsertNewTask(expire, interval_ms, true, std::move(cb));
}

bool TimingWheel::CancelTask(TimerId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        it = task_map_.find(id);
    if (it == task_map_.end())
        return false;
    it->second->cancelled = true;
    task_map_.erase(it);
    return true;
}

bool TimingWheel::IsTaskActive(TimerId id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return task_map_.find(id) != task_map_.end();
}

std::optional<std::chrono::milliseconds> TimingWheel::GetRemainingTime(TimerId id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        it = task_map_.find(id);
    if (it == task_map_.end())
        return std::nullopt;
    auto now       = std::chrono::steady_clock::now();
    auto remaining = it->second->expire_time - now;
    if (remaining.count() <= 0)
        return std::chrono::milliseconds(0);
    return std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
}

size_t TimingWheel::GetActiveTaskCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return task_map_.size();
}

void TimingWheel::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_.load(std::memory_order_acquire))
            return;
        stop_.store(true, std::memory_order_release);
        initialized_ = false;
        cv_.notify_all();
    }
    if (worker_thread_.joinable())
        worker_thread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    task_map_.clear();
    expired_tasks_.clear();
    levels_.clear();
}

bool TimingWheel::IsRunning() const
{ return initialized_ && !stop_.load(std::memory_order_acquire); }

uint64_t TimingWheel::GetMaxCapacityMs() const
{
    if (levels_.empty())
        return 0;
    uint64_t cap = base_tick_ms_;
    for (auto &l: levels_)
        cap *= l.slot_count;
    return cap;
}

TimerId TimingWheel::InsertNewTask(TimePoint expire, uint32_t interval, bool repeating, std::function<void()> cb)
{
    auto id            = GenerateNextId();
    auto task          = std::make_shared<TimerTask>();
    task->id           = id;
    task->callback     = std::move(cb);
    task->expire_time  = expire;
    task->interval_ms  = interval;
    task->is_repeating = repeating;
    task->cancelled    = false;
    task_map_[id]      = task;

    if (task_map_.size() == 1)
    {
        last_tick_time_ = std::chrono::steady_clock::now();
        cv_.notify_one();
    }
    InsertTask(task);
    return id;
}

TimerId TimingWheel::GenerateNextId()
{
    TimerId id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (id == kInvalidTimerId)
    {
        id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    return id;
}

void TimingWheel::InsertTask(const std::shared_ptr<TimerTask> &task)
{
    if (task->cancelled)
        return;
    int64_t distance =
            std::chrono::duration_cast<std::chrono::milliseconds>(task->expire_time - last_tick_time_).count();
    if (distance <= 0)
    {
        expired_tasks_.push_back(task);
        return;
    }
    for (size_t i = 0; i < levels_.size(); ++i)
    {
        auto    &level = levels_[i];
        uint64_t cap   = level.tick_interval_ms * level.slot_count;
        if (static_cast<uint64_t>(distance) < cap || i == levels_.size() - 1)
        {
            uint64_t ticks = distance / level.tick_interval_ms;
            uint32_t slot  = (level.current_index + static_cast<uint32_t>(ticks)) % level.slot_count;
            level.slots[slot].push_back(task);
            return;
        }
    }
}

void TimingWheel::Cascade(size_t level_idx)
{
    auto &level         = levels_[level_idx];
    level.current_index = (level.current_index + 1) % level.slot_count;
    auto tasks          = std::move(level.slots[level.current_index]);
    level.slots[level.current_index].clear();

    for (auto &task: tasks)
    {
        if (task->cancelled)
            continue;
        if (level_idx == 0)
        {
            expired_tasks_.push_back(std::move(task));
        }
        else
        {
            InsertTask(task);
        }
    }
    if (level.current_index == 0 && level_idx + 1 < levels_.size())
    {
        Cascade(level_idx + 1);
    }
}

void TimingWheel::WorkerLoop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop_.load(std::memory_order_acquire))
    {
        if (task_map_.empty() && expired_tasks_.empty())
        {
            cv_.wait(
                    lock, [this]
                    { return stop_.load(std::memory_order_acquire) || !task_map_.empty() || !expired_tasks_.empty(); });
            if (stop_.load(std::memory_order_acquire))
                break;
        }

        auto now  = std::chrono::steady_clock::now();
        auto tick = std::chrono::milliseconds(base_tick_ms_);
        while (last_tick_time_ + tick <= now)
        {
            last_tick_time_ += tick;
            Cascade(0);
        }

        if (!expired_tasks_.empty())
        {
            std::vector<std::shared_ptr<TimerTask>> to_run;
            to_run.swap(expired_tasks_);
            for (auto &t: to_run)
            {
                if (!t->is_repeating)
                    task_map_.erase(t->id);
            }

            lock.unlock();

            std::vector<std::shared_ptr<TimerTask>> repeating;
            for (auto &task: to_run)
            {
                if (!task->cancelled && task->callback)
                {
                    /**
                         * TODO: 将到期回调提交至全局线程池执行，避免阻塞时间轮驱动线程。
                         * 示例：
                         *   auto& pool = GetGlobalThreadPool();
                         *   pool.Enqueue([cb = task->callback]() { cb(); });
                         *
                         * 注意：如果回调执行过程中可能修改时间轮状态，需要确保线程安全，
                         * 回调内部适当加锁或通过消息队列异步解耦。
                         */
                    try
                    {
                        task->callback();
                    }
                    catch (...)
                    {
                        // 记录异常，避免影响时间轮
                    }
                }
                if (task->is_repeating && !task->cancelled)
                {
                    repeating.push_back(task);
                }
            }

            lock.lock();

            for (auto &task: repeating)
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

        if (!task_map_.empty())
        {
            auto next_tick = last_tick_time_ + std::chrono::milliseconds(base_tick_ms_);
            cv_.wait_until(lock, next_tick, [this] { return stop_.load(std::memory_order_acquire); });
        }
        else if (expired_tasks_.empty())
        {
            cv_.wait(
                    lock, [this]
                    { return stop_.load(std::memory_order_acquire) || !task_map_.empty() || !expired_tasks_.empty(); });
        }
    }
}

TimingWheel &GetGlobalTimingWheel()
{
    static TimingWheel instance;
    return instance;
}
}// namespace tyke
