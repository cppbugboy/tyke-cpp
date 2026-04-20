/**
 * @file timing_wheel.cpp
 * @brief 多级时间轮组件实现。支持动态添加多层时间轮，用于过期任务清理。
 * @author Nick
 * @date 2026/04/20
 */

#include "component/timing_wheel.h"
#include "common/log_def.h"
#include "core/request_stub.h"

namespace tyke
{
    TimingWheelLevel::TimingWheelLevel(uint32_t tick_interval_ms, uint32_t slot_count)
        : tick_interval_ms_(tick_interval_ms), slot_count_(slot_count), current_slot_(0)
    {
        slots_.resize(slot_count);
    }

    uint32_t TimingWheelLevel::GetTickIntervalMs() const { return tick_interval_ms_; }
    uint32_t TimingWheelLevel::GetSlotCount() const { return slot_count_; }
    uint32_t TimingWheelLevel::GetCurrentSlot() const { return current_slot_; }

    void TimingWheelLevel::AddTask(uint32_t slot, const TaskEntry& entry)
    {
        if (slot < slot_count_)
        {
            slots_[slot].push_back(entry);
        }
    }

    std::list<TaskEntry> TimingWheelLevel::AdvanceSlot()
    {
        current_slot_ = (current_slot_ + 1) % slot_count_;
        auto tasks = std::move(slots_[current_slot_]);
        slots_[current_slot_].clear();
        return tasks;
    }

    TimingWheelConfig TimingWheel::DefaultConfig()
    {
        TimingWheelConfig config;
        config.levels = {
            {200, 50},
            {1000, 60},
            {10000, 6},
            {60000, 60},
        };
        return config;
    }

    TimingWheel::~TimingWheel()
    {
        Stop();
    }

    void TimingWheel::Init(const TimingWheelConfig& config)
    {
        if (initialized_.exchange(true))
        {
            LOG_WARN("TimingWheel already initialized");
            return;
        }

        for (const auto& level_config : config.levels)
        {
            levels_.emplace_back(level_config.tick_interval_ms, level_config.slot_count);
        }

        stopped_.store(false);
        tick_thread_ = std::thread(&TimingWheel::TickLoop, this);
        LOG_INFO("TimingWheel initialized with {} levels", levels_.size());
    }

    void TimingWheel::AddTask(const std::string& uuid, uint32_t timeout_ms, TaskEntry::Type type)
    {
        if (stopped_.load() || levels_.empty())
        {
            LOG_WARN("TimingWheel not running, cannot add task, uuid={}", uuid);
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        size_t level_index = SelectLevel(timeout_ms);
        uint32_t slot = CalculateSlot(level_index, timeout_ms);

        TaskEntry entry;
        entry.uuid = uuid;
        entry.expire_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        entry.timeout_ms = timeout_ms;
        entry.type = type;

        levels_[level_index].AddTask(slot, entry);
        task_location_[uuid] = {level_index, slot};

        LOG_DEBUG("Task added to timing wheel, uuid={}, timeout={}ms, level={}, slot={}",
                  uuid, timeout_ms, level_index, slot);
    }

    void TimingWheel::RemoveTask(const std::string& uuid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        task_location_.erase(uuid);
        LOG_DEBUG("Task removed from timing wheel, uuid={}", uuid);
    }

    void TimingWheel::Stop()
    {
        if (stopped_.exchange(true))
            return;

        if (tick_thread_.joinable())
        {
            tick_thread_.join();
        }

        std::lock_guard<std::mutex> lock(mutex_);
        task_location_.clear();
        levels_.clear();
        initialized_.store(false);
        LOG_INFO("TimingWheel stopped");
    }

    size_t TimingWheel::SelectLevel(uint32_t timeout_ms) const
    {
        uint32_t max_range_ms = 0;
        for (size_t i = 0; i < levels_.size(); ++i)
        {
            max_range_ms += levels_[i].GetTickIntervalMs() * levels_[i].GetSlotCount();
            if (timeout_ms <= max_range_ms)
            {
                return i;
            }
        }
        return levels_.size() - 1;
    }

    uint32_t TimingWheel::CalculateSlot(size_t level_index, uint32_t timeout_ms) const
    {
        const auto& level = levels_[level_index];
        uint32_t tick_interval = level.GetTickIntervalMs();
        uint32_t ticks_ahead = (timeout_ms + tick_interval - 1) / tick_interval;
        uint32_t current_slot = level.GetCurrentSlot();
        uint32_t target_slot = (current_slot + ticks_ahead) % level.GetSlotCount();
        return target_slot;
    }

    void TimingWheel::TickLoop()
    {
        LOG_INFO("TimingWheel tick loop started");

        uint32_t tick_interval_ms = 200;
        if (!levels_.empty())
        {
            tick_interval_ms = levels_[0].GetTickIntervalMs();
        }

        while (!stopped_.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(tick_interval_ms));

            if (stopped_.load())
                break;

            std::lock_guard<std::mutex> lock(mutex_);

            for (size_t i = 0; i < levels_.size(); ++i)
            {
                auto tasks = levels_[i].AdvanceSlot();
                if (!tasks.empty())
                {
                    ProcessExpiredTasks(tasks);
                }

                if (levels_[i].GetCurrentSlot() != 0)
                {
                    break;
                }
            }
        }

        LOG_INFO("TimingWheel tick loop stopped");
    }

    void TimingWheel::ProcessExpiredTasks(std::list<TaskEntry>& tasks)
    {
        auto now = std::chrono::steady_clock::now();

        for (auto& entry : tasks)
        {
            auto it = task_location_.find(entry.uuid);
            if (it == task_location_.end())
            {
                continue;
            }

            if (entry.expire_time > now)
            {
                size_t level_index = SelectLevel(
                    static_cast<uint32_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(entry.expire_time - now).count()));
                uint32_t slot = CalculateSlot(level_index, entry.timeout_ms);
                levels_[level_index].AddTask(slot, entry);
                task_location_[entry.uuid] = {level_index, slot};
                continue;
            }

            task_location_.erase(entry.uuid);

            if (entry.type == TaskEntry::FUTURE)
            {
                LOG_WARN("TimingWheel: future task expired, uuid={}, timeout={}ms", entry.uuid, entry.timeout_ms);
                RequestStub::CleanupExpiredFuture(entry.uuid);
            }
            else
            {
                LOG_WARN("TimingWheel: func task expired, uuid={}, timeout={}ms", entry.uuid, entry.timeout_ms);
                RequestStub::CleanupExpiredFunc(entry.uuid);
            }
        }
    }
}
