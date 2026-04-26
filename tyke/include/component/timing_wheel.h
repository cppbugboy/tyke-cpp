/**
 * @file timing_wheel.h
 * @brief 工业级多级层次时间轮。
 *
 * 本文件实现了一个高效的多级层次时间轮，用于定时器管理。
 * 支持相对延迟、绝对截止时间和周期性任务。
 *
 * @section features 主要特性
 * - 多级分层设计，支持宽时间范围
 * - O(1)的添加和删除操作
 * - 支持一次性定时器和重复定时器
 * - 基于回调的到期处理
 * - 线程安全操作
 *
 * @section architecture 架构
 * 时间轮使用多个不同精度的层级：
 *   - 第0层: 基础刻度精度
 *   - 第1层: 更粗粒度的刻度
 *   - 以此类推...
 *
 * @section usage 使用示例
 * @code
 * auto& wheel = tyke::GetGlobalTimingWheel();
 * wheel.Init(10, {256, 64, 64, 64});
 *
 * // 添加一次性定时器
 * auto id = wheel.AddTask(5000, []() {
 *     std::cout << "Timer fired!" << std::endl;
 * });
 *
 * // 添加周期性定时器
 * wheel.AddRepeatedTask(1000, 500, []() {
 *     std::cout << "Repeating!" << std::endl;
 * });
 *
 * // 取消定时器
 * wheel.CancelTask(id);
 *
 * // 停止时间轮
 * wheel.Stop();
 * @endcode
 *
 * @author Nick
 * @date 2026/04/26
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
    using TimerId = uint64_t; ///< 定时器ID类型
    using TimePoint = std::chrono::steady_clock::time_point; ///< 时间点类型

    /**
     * @brief 定时器任务结构
     *
     * 表示一个已调度的定时器任务，包含回调函数、到期时间和重复配置。
     */
    struct TimerTask
    {
        TimerId id = 0; ///< 唯一标识符
        std::function<void()> callback; ///< 到期时调用的回调函数
        TimePoint expire_time; ///< 到期时间点
        uint32_t interval_ms = 0; ///< 重复间隔（毫秒），0表示一次性
        bool is_repeating = false; ///< 是否为重复定时器
        bool cancelled = false; ///< 是否已取消
    };

    /**
     * @brief 时间轮层级结构
     *
     * 表示时间轮的单个层级，包含多个槽位。
     */
    struct WheelLevel
    {
        uint64_t tick_interval_ms = 0; ///< 每刻度的毫秒数
        uint32_t slot_count = 0; ///< 槽位数量
        uint32_t current_index = 0; ///< 当前槽位索引
        std::vector<std::vector<std::shared_ptr<TimerTask>>> slots; ///< 槽位数组，每个槽位包含任务列表
    };

    /**
     * @brief 多级层次时间轮类
     *
     * 实现高效的多级时间轮，支持大范围的定时器调度。
     * 到期回调应提交至全局线程池执行，避免阻塞驱动线程。
     */
    class TimingWheel
    {
    public:
        /**
         * @brief 默认构造函数
         */
        TimingWheel() = default;

        /**
         * @brief 析构函数
         *
         * 自动调用 Stop() 停止时间轮。
         */
        ~TimingWheel()
        {
            Stop();
        }

        TimingWheel(const TimingWheel&) = delete;
        TimingWheel& operator=(const TimingWheel&) = delete;

        /**
         * @brief 初始化时间轮
         *
         * @param base_tick_ms 基础刻度（毫秒）
         * @param slots_per_level 每层槽位数组
         * @return true 初始化成功
         * @return false 初始化失败
         *
         * @code
         * wheel.Init(10, {256, 64, 64, 64});  // 10ms基础刻度，4层时间轮
         * @endcode
         */
        bool Init(uint32_t base_tick_ms = 10, const std::vector<uint32_t>& slots_per_level = {256, 64, 64, 64});

        /**
         * @brief 添加相对延迟任务
         *
         * @param delay_ms 延迟时间（毫秒）
         * @param cb 到期回调函数
         * @return TimerId 定时器ID，用于取消
         */
        [[nodiscard]] TimerId AddTask(uint32_t delay_ms, std::function<void()> cb);

        /**
         * @brief 添加绝对截止时间任务
         *
         * @param deadline 绝对截止时间点
         * @param cb 到期回调函数
         * @return TimerId 定时器ID，用于取消
         */
        [[nodiscard]] TimerId AddTaskAt(TimePoint deadline, std::function<void()> cb);

        /**
         * @brief 添加周期性任务
         *
         * @param initial_delay_ms 首次执行前的延迟（毫秒）
         * @param interval_ms 执行间隔（毫秒）
         * @param cb 回调函数
         * @return TimerId 定时器ID，用于取消
         */
        [[nodiscard]] TimerId AddRepeatedTask(uint32_t initial_delay_ms, uint32_t interval_ms,
                                              std::function<void()> cb);

        /**
         * @brief 取消定时器
         *
         * @param id 定时器ID
         * @return true 取消成功
         * @return false 定时器不存在或已执行
         */
        bool CancelTask(TimerId id);

        /**
         * @brief 检查定时器是否活跃
         *
         * @param id 定时器ID
         * @return true 定时器活跃
         * @return false 定时器不存在或已取消
         */
        bool IsTaskActive(TimerId id) const;

        /**
         * @brief 获取定时器剩余时间
         *
         * @param id 定时器ID
         * @return std::optional<std::chrono::milliseconds> 剩余时间，不存在返回nullopt
         */
        std::optional<std::chrono::milliseconds> GetRemainingTime(TimerId id) const;

        /**
         * @brief 获取活跃任务数量
         *
         * @return size_t 活跃任务数
         */
        size_t GetActiveTaskCount() const;

        /**
         * @brief 停止时间轮
         *
         * 停止驱动线程，取消所有定时器。
         */
        void Stop();

        /**
         * @brief 检查时间轮是否运行中
         *
         * @return true 运行中
         * @return false 已停止
         */
        bool IsRunning() const;

        /**
         * @brief 检查时间轮是否已初始化
         *
         * @return true 已初始化
         * @return false 未初始化
         */
        bool IsInitialized() const
        {
            return initialized_;
        }

        /**
         * @brief 获取基础刻度
         *
         * @return uint32_t 基础刻度（毫秒）
         */
        uint32_t GetBaseTickMs() const
        {
            return base_tick_ms_;
        }

        /**
         * @brief 获取最大容量（毫秒）
         *
         * @return uint64_t 时间轮能调度的最大延迟
         */
        uint64_t GetMaxCapacityMs() const;

        static constexpr TimerId kInvalidTimerId = 0; ///< 无效定时器ID

    private:
        /**
         * @brief 插入新任务
         *
         * @param expire 到期时间
         * @param interval 重复间隔
         * @param repeating 是否重复
         * @param cb 回调函数
         * @return TimerId 定时器ID
         */
        TimerId InsertNewTask(TimePoint expire, uint32_t interval, bool repeating, std::function<void()> cb);

        /**
         * @brief 生成下一个定时器ID
         *
         * @return TimerId 新的定时器ID
         */
        TimerId GenerateNextId();

        /**
         * @brief 将任务插入到适当的层级和槽位
         *
         * @param task 定时器任务
         */
        void InsertTask(const std::shared_ptr<TimerTask>& task);

        /**
         * @brief 层级级联
         *
         * 将高层级任务降级到低层级。
         *
         * @param level_idx 层级索引
         */
        void Cascade(size_t level_idx);

        /**
         * @brief 驱动线程主循环
         */
        void WorkerLoop();

        uint32_t base_tick_ms_ = 0; ///< 基础刻度（毫秒）
        std::atomic<bool> initialized_{false}; ///< 是否已初始化
        std::atomic<bool> stop_{true}; ///< 停止标志
        std::atomic<TimerId> next_id_{0}; ///< 下一个定时器ID
        std::vector<WheelLevel> levels_; ///< 时间轮层级
        TimePoint last_tick_time_; ///< 上次刻度时间
        std::thread worker_thread_; ///< 驱动线程
        mutable std::mutex mutex_; ///< 互斥锁
        std::condition_variable cv_; ///< 条件变量
        std::unordered_map<TimerId, std::shared_ptr<TimerTask>> task_map_; ///< 任务映射
        std::vector<std::shared_ptr<TimerTask>> expired_tasks_; ///< 已到期任务列表
    };

    /**
     * @brief 获取全局时间轮单例
     *
     * 返回进程级别的时间轮单例，需先调用 Init() 初始化。
     *
     * @return TimingWheel& 全局时间轮引用
     *
     * @code
     * auto& wheel = tyke::GetGlobalTimingWheel();
     * wheel.Init();
     * @endcode
     */
    TimingWheel& GetGlobalTimingWheel();
} // namespace tyke
