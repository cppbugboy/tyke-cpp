/**
 * @file timing_wheel.h
 * @brief 工业级多级层次时间轮 (Hierarchical Timing Wheel)
 *
 * 支持两种定时方式:
 *   1. Timeout  —— 相对延迟添加任务 (AddTask)
 *   2. Deadline —— 绝对截止时间添加任务 (AddTaskAt)
 *
 * 额外支持:
 *   - 周期性任务 (AddRepeatedTask)
 *   - 任务状态查询 (IsTaskActive / GetRemainingTime)
 *   - 异常安全的回调执行
 *   - C++17 / Google 编码风格
 *
 * 使用方式:
 *   TimingWheel wheel;
 *   wheel.Init(10, {256, 64, 64, 64});
 *   auto id = wheel.AddTask(100, [](){ ... });
 *
 * @author Optimized C++17 Implementation
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

namespace tyke {

// ============================================================================
// 类型别名
// ============================================================================

/// 定时器唯一标识符，0 表示无效 ID
using TimerId = uint64_t;

/// 时间点类型，基于 steady_clock 保证单调递增
using TimePoint = std::chrono::steady_clock::time_point;

// ============================================================================
// TimerTask —— 定时器任务单元
// ============================================================================

struct TimerTask {
  TimerId id{0};                        ///< 任务唯一标识符
  std::function<void()> callback;       ///< 到期回调函数
  TimePoint expire_time;                ///< 绝对到期时间点
  uint32_t interval_ms{0};             ///< 周期任务的重复间隔（毫秒），0 表示一次性任务
  bool is_repeating{false};             ///< 是否为周期性任务
  bool cancelled{false};                ///< 懒删除标记，true 表示已取消
};

// ============================================================================
// WheelLevel —— 时间轮单层级描述
// ============================================================================

struct WheelLevel {
  uint64_t tick_interval_ms{0};         ///< 当前层级单个 tick 的毫秒跨度
  uint32_t slot_count{0};               ///< 当前层级的槽位数量
  uint32_t current_index{0};            ///< 当前拨盘指针位置
  std::vector<std::vector<std::shared_ptr<TimerTask>>> slots; ///< 槽位数组，每槽存放任务列表
};

// ============================================================================
// TimingWheel —— 多级层次时间轮
// ============================================================================

class TimingWheel {
 public:
  // --------------------------------------------------------------------------
  // 构造 / 析构
  // --------------------------------------------------------------------------

  /**
   * @brief 默认构造函数，创建未初始化的时间轮对象
   *
   * 构造后需调用 Init() 进行初始化才能使用。
   * 在 Init() 调用之前，所有任务操作将返回 kInvalidTimerId 或 false。
   */
  TimingWheel() = default;

  /**
   * @brief 析构时自动停止时间轮并等待工作线程退出
   */
  ~TimingWheel() {
    Stop();
  }

  // 禁止拷贝
  TimingWheel(const TimingWheel&) = delete;
  TimingWheel& operator=(const TimingWheel&) = delete;

  // 禁止移动（工作线程持有 this 指针，移动会导致悬垂引用）
  TimingWheel(TimingWheel&&) = delete;
  TimingWheel& operator=(TimingWheel&&) = delete;

  // --------------------------------------------------------------------------
  // 初始化接口
  // --------------------------------------------------------------------------

  /**
   * @brief 初始化并启动时间轮
   * @param base_tick_ms   基础 tick 精度（毫秒），默认 10ms
   * @param slots_per_level 各层级槽位数，默认 {256, 64, 64, 64}
   * @return true 初始化成功，false 初始化失败（已初始化或参数无效）
   *
   * 初始化后时间轮立即进入运行状态，后台工作线程开始驱动时间轮推进。
   * 不可重复调用，若需重新配置须先 Stop() 再 Init()。
   *
   * 示例: base=10, slots={256, 64, 64, 64}
   *   L0: 10ms × 256           = 2,560ms       (2.56秒)
   *   L1: 2,560ms × 64         = 163,840ms     (约2.7分钟)
   *   L2: 163,840ms × 64       = 10,485,760ms  (约2.9小时)
   *   L3: 10,485,760ms × 64    = 671,088,640ms (约7.8天)
   */
  bool Init(uint32_t base_tick_ms = 10,
            const std::vector<uint32_t>& slots_per_level = {256, 64, 64, 64}) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 已初始化，拒绝重复调用
    if (initialized_) {
      return false;
    }

    // 参数校验
    if (base_tick_ms == 0 || slots_per_level.empty()) {
      return false;
    }

    base_tick_ms_ = base_tick_ms;

    // 逐级构建时间轮层级
    levels_.clear();
    levels_.reserve(slots_per_level.size());
    uint64_t current_tick_ms = base_tick_ms;
    for (const uint32_t slot_count : slots_per_level) {
      if (slot_count == 0) {
        return false;
      }
      WheelLevel level;
      level.tick_interval_ms = current_tick_ms;
      level.slot_count = slot_count;
      level.current_index = 0;
      level.slots.resize(slot_count);
      levels_.push_back(std::move(level));

      // 下一层的 tick 跨度 = 当前层的总容量
      current_tick_ms *= slot_count;
    }

    last_tick_time_ = std::chrono::steady_clock::now();
    stop_.store(false, std::memory_order_release);
    initialized_ = true;

    // 启动后台工作线程
    worker_thread_ = std::thread(&TimingWheel::WorkerLoop, this);

    return true;
  }

  // --------------------------------------------------------------------------
  // 任务管理接口 —— Timeout 方式（相对延迟）
  // --------------------------------------------------------------------------

  /**
   * @brief 添加一次性定时任务（Timeout 方式 —— 相对延迟）
   * @param delay_ms 延迟毫秒数，从当前时刻起计算
   * @param cb       到期执行的回调函数
   * @return TimerId 任务唯一句柄，0 表示添加失败（未初始化/已停止/回调为空）
   *
   * 适用场景: "在 N 毫秒后执行一次"
   */
  [[nodiscard]] TimerId AddTask(uint32_t delay_ms, std::function<void()> cb) {
    if (!cb) {
      return kInvalidTimerId;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 未初始化或已停止，拒绝添加
    if (!initialized_ || stop_.load(std::memory_order_acquire)) {
      return kInvalidTimerId;
    }

    const auto expire_time = std::chrono::steady_clock::now()
                           + std::chrono::milliseconds(delay_ms);
    return InsertNewTask(expire_time, /*interval_ms=*/0, /*is_repeating=*/false, std::move(cb));
  }

  // --------------------------------------------------------------------------
  // 任务管理接口 —— Deadline 方式（绝对时间点）
  // --------------------------------------------------------------------------

  /**
   * @brief 添加一次性定时任务（Deadline 方式 —— 绝对截止时间）
   * @param deadline 绝对到期时间点（steady_clock::time_point）
   * @param cb       到期执行的回调函数
   * @return TimerId 任务唯一句柄，0 表示添加失败
   *
   * 适用场景: "在某个精确时间点执行一次"
   *
   * 与 AddTask 的区别:
   *   - AddTask(delay_ms)  → 内部计算 expire_time = now() + delay_ms
   *   - AddTaskAt(deadline)→ 直接使用 deadline 作为 expire_time
   *
   * Deadline 方式可以避免因锁等待、线程调度等导致的延迟累积误差，
   * 适用于对触发精度要求严格的场景。
   */
  [[nodiscard]] TimerId AddTaskAt(TimePoint deadline, std::function<void()> cb) {
    if (!cb) {
      return kInvalidTimerId;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 未初始化或已停止，拒绝添加
    if (!initialized_ || stop_.load(std::memory_order_acquire)) {
      return kInvalidTimerId;
    }

    return InsertNewTask(deadline, /*interval_ms=*/0, /*is_repeating=*/false, std::move(cb));
  }

  // --------------------------------------------------------------------------
  // 任务管理接口 —— 周期性任务
  // --------------------------------------------------------------------------

  /**
   * @brief 添加周期性定时任务
   * @param initial_delay_ms 首次执行的延迟毫秒数
   * @param interval_ms      每次执行的间隔毫秒数
   * @param cb               每次到期执行的回调函数
   * @return TimerId 任务唯一句柄，0 表示添加失败
   *
   * 适用场景: "每隔 N 毫秒重复执行"
   *
   * 周期任务的重调度采用"基于上次到期时间 + 间隔"的策略，
   * 而非"基于当前时间 + 间隔"，从而避免执行延迟的累积漂移。
   * 例如: 间隔100ms的任务，若第1次在110ms时才执行（延迟10ms），
   * 下次仍在200ms时触发，而非210ms，保持节奏稳定。
   */
  [[nodiscard]] TimerId AddRepeatedTask(uint32_t initial_delay_ms,
                                        uint32_t interval_ms,
                                        std::function<void()> cb) {
    if (!cb || interval_ms == 0) {
      return kInvalidTimerId;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 未初始化或已停止，拒绝添加
    if (!initialized_ || stop_.load(std::memory_order_acquire)) {
      return kInvalidTimerId;
    }

    const auto expire_time = std::chrono::steady_clock::now()
                           + std::chrono::milliseconds(initial_delay_ms);
    return InsertNewTask(expire_time, interval_ms, /*is_repeating=*/true, std::move(cb));
  }

  // --------------------------------------------------------------------------
  // 任务控制接口
  // --------------------------------------------------------------------------

  /**
   * @brief 取消定时任务（O(1) 复杂度，懒删除）
   * @param id 任务唯一句柄
   * @return true 表示成功取消，false 表示任务不存在或已执行完毕
   *
   * 采用懒删除策略: 仅设置 cancelled 标记并从 task_map_ 移除引用，
   * 任务实体仍在时间轮槽位中，但执行时会被跳过。
   * 这避免了从槽位链表中定位并移除的 O(n) 开销。
   */
  bool CancelTask(TimerId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = task_map_.find(id);
    if (it == task_map_.end()) {
      return false;
    }
    it->second->cancelled = true;
    task_map_.erase(it);
    return true;
  }

  // --------------------------------------------------------------------------
  // 查询接口
  // --------------------------------------------------------------------------

  /**
   * @brief 查询任务是否仍处于活跃状态
   * @param id 任务唯一句柄
   * @return true 表示任务尚未到期且未被取消
   */
  bool IsTaskActive(TimerId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return task_map_.find(id) != task_map_.end();
  }

  /**
   * @brief 获取任务距到期的剩余时间
   * @param id 任务唯一句柄
   * @return 剩余时间（毫秒），std::nullopt 表示任务不存在或已完成
   *
   * 注意: 返回值为近似值，实际触发精度受 base_tick_ms 粒度限制。
   */
  std::optional<std::chrono::milliseconds> GetRemainingTime(TimerId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = task_map_.find(id);
    if (it == task_map_.end()) {
      return std::nullopt;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto remaining = it->second->expire_time - now;
    if (remaining.count() <= 0) {
      return std::chrono::milliseconds(0);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
  }

  /**
   * @brief 获取当前活跃任务数（含尚未到期的周期性任务）
   */
  [[nodiscard]] size_t GetActiveTaskCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return task_map_.size();
  }

  // --------------------------------------------------------------------------
  // 生命周期接口
  // --------------------------------------------------------------------------

  /**
   * @brief 停止时间轮
   *
   * 设置停止标志 → 唤醒工作线程 → 等待工作线程退出 → 重置初始化状态。
   * 已到期但尚未执行的回调会被丢弃。
   * 可安全多次调用。
   * 停止后可再次调用 Init() 重新初始化。
   */
  void Stop() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stop_.load(std::memory_order_acquire)) {
        return;
      }
      stop_.store(true, std::memory_order_release);
      initialized_ = false;
      cv_.notify_all();
    }
    if (worker_thread_.joinable()) {
      worker_thread_.join();
    }
    // 清理残留数据，为可能的重新 Init() 做准备
    std::lock_guard<std::mutex> lock(mutex_);
    task_map_.clear();
    expired_tasks_.clear();
    levels_.clear();
  }

  /**
   * @brief 查询时间轮是否处于运行状态
   */
  [[nodiscard]] bool IsRunning() const {
    return initialized_ && !stop_.load(std::memory_order_acquire);
  }

  /**
   * @brief 查询时间轮是否已完成初始化
   */
  [[nodiscard]] bool IsInitialized() const {
    return initialized_;
  }

  /**
   * @brief 获取时间轮的基础 tick 精度（毫秒）
   */
  [[nodiscard]] uint32_t GetBaseTickMs() const {
    return base_tick_ms_;
  }

  /**
   * @brief 计算时间轮最大覆盖时长（毫秒）
   * @return 所有时级联后可覆盖的最大延迟毫秒数，未初始化时返回 0
   */
  [[nodiscard]] uint64_t GetMaxCapacityMs() const {
    if (levels_.empty()) {
      return 0;
    }
    uint64_t capacity = base_tick_ms_;
    for (const auto& level : levels_) {
      capacity *= level.slot_count;
    }
    return capacity;
  }

 private:
  // --------------------------------------------------------------------------
  // 内部常量
  // --------------------------------------------------------------------------

  static constexpr TimerId kInvalidTimerId = 0; ///< 无效定时器 ID

  // --------------------------------------------------------------------------
  // 内部方法 —— 任务插入
  // --------------------------------------------------------------------------

  /**
   * @brief 创建并插入新任务（需在锁保护下调用）
   * @param expire_time  绝对到期时间点
   * @param interval_ms  周期间隔（0 表示一次性任务）
   * @param is_repeating 是否为周期性任务
   * @param cb           回调函数
   * @return TimerId 任务唯一标识
   */
  TimerId InsertNewTask(TimePoint expire_time,
                        uint32_t interval_ms,
                        bool is_repeating,
                        std::function<void()> cb) {
    const TimerId id = GenerateNextId();

    auto task = std::make_shared<TimerTask>();
    task->id = id;
    task->callback = std::move(cb);
    task->expire_time = expire_time;
    task->interval_ms = interval_ms;
    task->is_repeating = is_repeating;
    task->cancelled = false;

    // 记录到 task_map_，兼具快速查找与保活双重职责
    task_map_[id] = task;

    // 若之前无任务，重置基准时间并唤醒休眠的工作线程
    const bool was_empty = task_map_.size() == 1;
    if (was_empty) {
      last_tick_time_ = std::chrono::steady_clock::now();
      cv_.notify_one();
    }

    InsertTask(task);
    return id;
  }

  /**
   * @brief 生成下一个递增的 TimerId
   * @return 全局唯一的任务标识符，保证不为 0
   *
   * 使用原子操作保证线程安全。当 ID 溢出回绕时跳过 0（无效 ID），
   * 极端情况下（2^64 个 ID 耗尽）回绕到 1 重新开始，
   * 由于旧任务早已执行完毕，冲突概率极低。
   */
  TimerId GenerateNextId() {
    TimerId id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    // 跳过无效 ID（0），理论上仅在 uint64_t 溢出时触发
    if (id == kInvalidTimerId) {
      id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    return id;
  }

  /**
   * @brief 将任务插入到时间轮合适的层级和槽位（需在锁保护下调用）
   * @param task 待插入的任务
   *
   * 插入策略:
   *   1. 计算任务到期时间与当前 tick 的时间距离
   *   2. 如果已过期，直接放入待执行队列
   *   3. 从最低层开始逐级查找，选择第一个能容纳该距离的层级
   *   4. 在选定层级中计算具体槽位: (当前指针 + 偏移 tick 数) % 槽位数
   *   5. 若所有层级均无法容纳（距离超出最大容量），放入最高层兜底
   */
  void InsertTask(const std::shared_ptr<TimerTask>& task) {
    if (task->cancelled) {
      return;
    }

    // 计算任务距时间轮当前基准的时间距离
    const int64_t distance_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            task->expire_time - last_tick_time_).count();

    // 已过期的任务直接放入待执行队列
    if (distance_ms <= 0) {
      expired_tasks_.push_back(task);
      return;
    }

    // 逐级查找合适的插入层级
    for (size_t i = 0; i < levels_.size(); ++i) {
      auto& level = levels_[i];
      const uint64_t max_capacity_ms = level.tick_interval_ms * level.slot_count;

      // 距离落在当前层级范围内，或为最高层（兜底）
      if (static_cast<uint64_t>(distance_ms) < max_capacity_ms
          || i == levels_.size() - 1) {
        const uint64_t ticks = distance_ms / level.tick_interval_ms;
        const uint32_t slot = (level.current_index
                             + static_cast<uint32_t>(ticks)) % level.slot_count;
        level.slots[slot].push_back(task);
        return;
      }
    }
  }

  // --------------------------------------------------------------------------
  // 内部方法 —— 层级降级（Cascade）
  // --------------------------------------------------------------------------

  /**
   * @brief 推进指定层级的拨盘，并处理到期/降级任务
   * @param level_idx 层级索引（0 为最底层）
   *
   * Cascade 是时间轮的核心驱动机制，工作流程:
   *   1. 将当前层级指针前进一步
   *   2. 取出新指针位置槽位中的所有任务
   *   3. 对于最底层(L0): 任务已精准到期，移入 expired_tasks_
   *   4. 对于高层级(L1+): 任务临近到期，重新计算距离插入底层（层级降级）
   *   5. 若当前层级转满一圈（指针回到0），递归驱动上一层级转动一格
   *
   * 类比时钟: 秒针走完一圈 → 分针进一格 → 分针槽位中的任务分配到秒针。
   */
  void Cascade(size_t level_idx) {
    auto& level = levels_[level_idx];

    // 拨盘指针前进一格
    level.current_index = (level.current_index + 1) % level.slot_count;

    // 取出当前槽位中的所有任务（零拷贝转移）
    auto tasks = std::move(level.slots[level.current_index]);
    level.slots[level.current_index].clear();

    for (auto& task : tasks) {
      // 跳过已取消的任务（懒删除）
      if (task->cancelled) {
        continue;
      }

      if (level_idx == 0) {
        // 最底层任务已精准到期，放入待执行队列
        expired_tasks_.push_back(std::move(task));
      } else {
        // 高层任务临近到期，降级插入到更底层
        InsertTask(task);
      }
    }

    // 当前层级转满一圈，触发上一层推进一格
    if (level.current_index == 0 && level_idx + 1 < levels_.size()) {
      Cascade(level_idx + 1);
    }
  }

  // --------------------------------------------------------------------------
  // 内部方法 —— 工作线程主循环
  // --------------------------------------------------------------------------

  /**
   * @brief 后台工作线程主循环
   *
   * 核心职责:
   *   1. 空闲检测: 无任务时休眠等待唤醒，节省 CPU
   *   2. Tick 推进: 按 base_tick_ms 间隔驱动最底层 Cascade
   *   3. 追赶补偿: 处理系统休眠/高负载导致的丢失 tick
   *   4. 过期执行: 释放锁后执行回调，避免死锁
   *   5. 周期重调度: 周期任务执行后基于上次到期时间重新插入
   *
   * 线程安全说明:
   *   - 全程持有 mutex_，仅在执行回调时短暂释放
   *   - 回调中可以安全调用 AddTask / CancelTask 等接口
   */
  void WorkerLoop() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (!stop_.load(std::memory_order_acquire)) {
      // ----------------------------------------------------------
      // 阶段1: 空闲休眠
      //   当没有任何任务时，阻塞等待直到新任务到来或停止信号
      // ----------------------------------------------------------
      if (task_map_.empty() && expired_tasks_.empty()) {
        cv_.wait(lock, [this]() {
          return stop_.load(std::memory_order_acquire)
              || !task_map_.empty()
              || !expired_tasks_.empty();
        });
        if (stop_.load(std::memory_order_acquire)) {
          break;
        }
      }

      // ----------------------------------------------------------
      // 阶段2: Tick 推进与追赶
      //   补偿因系统休眠或高负载导致丢失的 tick，
      //   确保 Cascade 调用次数与流逝的时间严格对应。
      // ----------------------------------------------------------
      const auto now = std::chrono::steady_clock::now();
      const auto tick_duration = std::chrono::milliseconds(base_tick_ms_);

      while (last_tick_time_ + tick_duration <= now) {
        last_tick_time_ += tick_duration;
        Cascade(0); // 驱动最底层齿轮
      }

      // ----------------------------------------------------------
      // 阶段3: 处理到期任务
      //   分类处理一次性任务和周期性任务，
      //   释放锁后执行回调（防止死锁），
      //   周期任务基于上次到期时间重新调度。
      // ----------------------------------------------------------
      if (!expired_tasks_.empty()) {
        std::vector<std::shared_ptr<TimerTask>> tasks_to_run;
        tasks_to_run.swap(expired_tasks_);

        // 从 task_map_ 中移除一次性任务；周期任务保留映射
        for (const auto& task : tasks_to_run) {
          if (!task->is_repeating) {
            task_map_.erase(task->id);
          }
        }

        // 【核心安全设计】释放锁 → 执行回调 → 重新获取锁
        // 确保回调中的 AddTask/CancelTask 等操作不会死锁
        lock.unlock();

        // 分离周期性任务用于后续重调度
        std::vector<std::shared_ptr<TimerTask>> repeating_to_reschedule;

        for (const auto& task : tasks_to_run) {
          if (!task->cancelled && task->callback) {
            try {
              task->callback();
            } catch (...) {
              // 吞掉回调异常，防止工作线程崩溃
              // 生产环境中应接入日志系统记录异常详情
            }
          }
          // 收集需要重调度的周期性任务
          if (task->is_repeating && !task->cancelled) {
            repeating_to_reschedule.push_back(task);
          }
        }

        lock.lock();

        // 重新调度周期性任务
        // 采用 expire_time + interval_ms 而非 now + interval_ms，
        // 避免执行延迟的累积漂移
        for (auto& task : repeating_to_reschedule) {
          // 再次检查取消状态（可能在回调执行期间被取消）
          if (task->cancelled) {
            task_map_.erase(task->id);
            continue;
          }
          task->expire_time += std::chrono::milliseconds(task->interval_ms);
          InsertTask(task);
          // task_map_ 中仍保留该任务的映射，无需重新添加
        }
      }

      if (stop_.load(std::memory_order_acquire)) {
        break;
      }

      // ----------------------------------------------------------
      // 阶段4: 等待下一个 Tick
      //   使用 wait_until 精确等待到下一个 tick 时间点，
      //   同时响应停止信号和新任务唤醒。
      // ----------------------------------------------------------
      if (!task_map_.empty()) {
        const auto next_tick_time = last_tick_time_
                                  + std::chrono::milliseconds(base_tick_ms_);
        cv_.wait_until(lock, next_tick_time, [this]() {
          return stop_.load(std::memory_order_acquire);
        });
      } else if (expired_tasks_.empty()) {
        // 无任务时回到休眠状态
        cv_.wait(lock, [this]() {
          return stop_.load(std::memory_order_acquire)
              || !task_map_.empty()
              || !expired_tasks_.empty();
        });
      }
    }
  }

  // --------------------------------------------------------------------------
  // 数据成员
  // --------------------------------------------------------------------------

  uint32_t base_tick_ms_{0};            ///< 基础 tick 精度（毫秒），Init() 前为 0
  bool initialized_{false};             ///< 初始化状态标志，Init() 后为 true
  std::atomic<bool> stop_{true};        ///< 停止标志（构造前为 true）
  std::atomic<TimerId> next_id_{0};     ///< 下一个可用 TimerId（原子递增）

  std::vector<WheelLevel> levels_;      ///< 各层级时间轮
  TimePoint last_tick_time_;            ///< 上次 tick 的基准时间点

  std::thread worker_thread_;           ///< 后台工作线程
  mutable std::mutex mutex_;            ///< 全局互斥锁（mutable 支持 const 方法加锁）
  std::condition_variable cv_;          ///< 条件变量（用于精确等待和唤醒）

  std::unordered_map<TimerId, std::shared_ptr<TimerTask>> task_map_; ///< 任务查找表 + 保活容器
  std::vector<std::shared_ptr<TimerTask>> expired_tasks_;            ///< 已到期待执行的任务队列
};

} // namespace tyke
