/**
 * @file timing_wheel.h
 * @brief 工业级多级层次时间轮。
 *
 * 支持相对延迟 (AddTask)、绝对截止时间 (AddTaskAt) 及周期性任务 (AddRepeatedTask)。
 * 到期回调应提交至全局线程池执行，避免阻塞驱动线程。
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace tyke
{
using TimerId   = uint64_t;
using TimePoint = std::chrono::steady_clock::time_point;

struct TimerTask
{
    TimerId               id = 0;
    std::function<void()> callback;
    TimePoint             expire_time;
    uint32_t              interval_ms  = 0;
    bool                  is_repeating = false;
    bool                  cancelled    = false;
};

struct WheelLevel
{
    uint64_t                                             tick_interval_ms = 0;
    uint32_t                                             slot_count       = 0;
    uint32_t                                             current_index    = 0;
    std::vector<std::vector<std::shared_ptr<TimerTask>>> slots;
};

class TimingWheel
{
public:
    TimingWheel() = default;
    ~TimingWheel()
    { Stop(); }

    TimingWheel(const TimingWheel &)            = delete;
    TimingWheel &operator=(const TimingWheel &) = delete;

    bool Init(uint32_t base_tick_ms = 10, const std::vector<uint32_t> &slots_per_level = {256, 64, 64, 64});

    [[nodiscard]] TimerId AddTask(uint32_t delay_ms, std::function<void()> cb);
    [[nodiscard]] TimerId AddTaskAt(TimePoint deadline, std::function<void()> cb);
    [[nodiscard]] TimerId AddRepeatedTask(uint32_t initial_delay_ms, uint32_t interval_ms, std::function<void()> cb);
    bool                  CancelTask(TimerId id);
    bool                  IsTaskActive(TimerId id) const;
    std::optional<std::chrono::milliseconds> GetRemainingTime(TimerId id) const;
    size_t                                   GetActiveTaskCount() const;
    void                                     Stop();
    bool                                     IsRunning() const;
    bool                                     IsInitialized() const
    { return initialized_; }
    uint32_t GetBaseTickMs() const
    { return base_tick_ms_; }
    uint64_t GetMaxCapacityMs() const;

private:
    static constexpr TimerId kInvalidTimerId = 0;

    TimerId InsertNewTask(TimePoint expire, uint32_t interval, bool repeating, std::function<void()> cb);
    TimerId GenerateNextId();
    void    InsertTask(const std::shared_ptr<TimerTask> &task);
    void    Cascade(size_t level_idx);
    void    WorkerLoop();

    uint32_t                                                base_tick_ms_ = 0;
    bool                                                    initialized_  = false;
    std::atomic<bool>                                       stop_{true};
    std::atomic<TimerId>                                    next_id_{0};
    std::vector<WheelLevel>                                 levels_;
    TimePoint                                               last_tick_time_;
    std::thread                                             worker_thread_;
    mutable std::mutex                                      mutex_;
    std::condition_variable                                 cv_;
    std::unordered_map<TimerId, std::shared_ptr<TimerTask>> task_map_;
    std::vector<std::shared_ptr<TimerTask>>                 expired_tasks_;
};

/** @brief 全局时间轮访问函数，需先调用 Init()。 */
TimingWheel &GetGlobalTimingWheel();
}// namespace tyke
