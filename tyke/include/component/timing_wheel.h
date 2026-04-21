/**
 * @file timing_wheel.h
 * @brief 多级时间轮组件声明。支持动态添加多层时间轮，用于过期任务清理。
 * @author Nick
 * @date 2026/04/20
 *
 * 多级时间轮设计：
 * - Level 0: 200ms精度, 50个槽位 (0-9.8s)
 * - Level 1: 1s精度, 10个槽位 (10s-59s)
 * - Level 2: 10s精度, 6个槽位 (1min-9min50s)
 * - Level 3: 1min精度, 60个槽位 (10min-59min)
 */


#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "component/singleton.h"

namespace tyke
{
    struct TaskEntry
    {
        std::string uuid;
        std::chrono::steady_clock::time_point expire_time;
        uint32_t timeout_ms;
        enum Type
        {
            kFunc,
            kFuture
        } type;
    };

    struct TimingWheelLevelConfig
    {
        uint32_t tick_interval_ms;
        uint32_t slot_count;
    };

    struct TimingWheelConfig
    {
        std::vector<TimingWheelLevelConfig> levels;
    };

    class TimingWheelLevel
    {
    public:
        TimingWheelLevel(uint32_t tick_interval_ms, uint32_t slot_count);

        uint32_t GetTickIntervalMs() const;
        uint32_t GetSlotCount() const;
        uint32_t GetCurrentSlot() const;

        void AddTask(uint32_t slot, const TaskEntry& entry);
        std::list<TaskEntry> AdvanceSlot();

    private:
        uint32_t tick_interval_ms_;
        uint32_t slot_count_;
        uint32_t current_slot_ = 0;
        std::vector<std::list<TaskEntry>> slots_;
    };

    class TimingWheel : public Singleton<TimingWheel>
    {
        friend class Singleton<TimingWheel>;

    public:
        void Init(const TimingWheelConfig& config = DefaultConfig());

        void AddTask(const std::string& uuid, uint32_t timeout_ms, TaskEntry::Type type);

        void RemoveTask(const std::string& uuid);

        void Stop();

        static TimingWheelConfig DefaultConfig();

    private:
        TimingWheel() = default;
        ~TimingWheel() override;

        void TickLoop();
        void ProcessExpiredTasks(std::list<TaskEntry>& tasks);
        size_t SelectLevel(uint32_t timeout_ms) const;
        uint32_t CalculateSlot(size_t level_index, uint32_t timeout_ms) const;

        std::vector<TimingWheelLevel> levels_;
        std::thread tick_thread_;
        std::atomic<bool> stopped_{true};
        std::atomic<bool> initialized_{false};
        std::mutex mutex_;
        std::unordered_map<std::string, std::pair<size_t, uint32_t>> task_location_;
    };
}
