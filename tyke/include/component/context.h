/**
 * @file context.h
 * @brief 上下文体系：取消、超时、值传递。
 *
 * 提供类似 Go 的 Context 抽象，支持组合取消树、截止时间与键值携带。
 * 通过 ContextPool 实现对象复用，降低高频创建开销。
 *
 * 与线程池/时间轮的集成说明：
 *   - 当 Context 需要超时或取消传播时，应在适当位置将任务提交到
 *     全局时间轮和线程池（参见代码中的 TODO 注释）。
 */

#pragma once

#include <any>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "object_pool.h"

namespace tyke
{
// ----------------------------- 错误码 -----------------------------
enum class ContextError
{
    kNone = 0,
    kCanceled,
    kDeadlineExceeded,
};

// ----------------------------- 前置声明 -----------------------------
class Context;
using ContextPtr = std::shared_ptr<Context>;

using CancelToken                         = uint64_t;
constexpr CancelToken kInvalidCancelToken = 0;

// ----------------------------- 取消状态（共享） -----------------------------
struct CancelState
{
    mutable std::mutex                                     mu;
    mutable std::condition_variable                        cv;
    std::atomic<bool>                                      atomic_done{false};
    ContextError                                           err{ContextError::kNone};
    std::atomic<CancelToken>                               next_token{1};
    std::unordered_map<CancelToken, std::function<void()>> callbacks;

    void Reset()
    {
        std::lock_guard<std::mutex> lock(mu);
        atomic_done = false;
        err         = ContextError::kNone;
        next_token  = 1;
        callbacks.clear();
    }
};

using CancelStatePtr = std::shared_ptr<CancelState>;

// ============================================================================
// 核心接口
// ============================================================================
class Context
{
public:
    virtual ~Context() = default;

    // 禁止拷贝，允许移动（子类自行决定是否支持移动）
    Context(const Context &)            = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&)                 = delete;
    Context &operator=(Context &&)      = delete;

    [[nodiscard]] virtual std::optional<std::chrono::system_clock::time_point> Deadline() const             = 0;
    [[nodiscard]] virtual bool                                                 IsDone() const               = 0;
    [[nodiscard]] virtual ContextError                                         Err() const                  = 0;
    virtual void                                                               Wait() const                 = 0;
    virtual std::any                                                           Value(const void *key) const = 0;

    /** @brief 重置对象状态，供对象池使用。 */
    virtual void Reset() = 0;

protected:
    Context() = default;
};

// ============================================================================
// 1. 空上下文（Background / TODO）
// ============================================================================
class EmptyContext final : public Context
{
public:
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override;
    [[nodiscard]] bool                                                 IsDone() const override;
    [[nodiscard]] ContextError                                         Err() const override;
    void                                                               Wait() const override;
    std::any                                                           Value(const void *key) const override;
    void                                                               Reset() override;
};

// ============================================================================
// 2. 可取消上下文
// ============================================================================
class CancelContext : public Context, public std::enable_shared_from_this<CancelContext>
{
public:
    CancelContext();
    ~CancelContext() override = default;

    /**
         * @brief 从对象池初始化。
         * @param parent 父上下文，可为 nullptr。
         *
         * 如果父上下文已结束，则立即传播取消状态。
         */
    void Init(ContextPtr parent);

    void Reset() override;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override;
    [[nodiscard]] bool                                                 IsDone() const override;
    [[nodiscard]] ContextError                                         Err() const override;
    void                                                               Wait() const override;
    std::any                                                           Value(const void *key) const override;

    /**
         * @brief 触发取消，调用所有注册的回调。
         *
         * TODO: 遍历回调时，建议将回调提交到全局线程池异步执行，
         *       避免阻塞调用线程，并防止回调中加锁导致死锁。
         */
    void Cancel(ContextError err) const;

    /** @brief 注册取消回调，返回 token 可用于注销。 */
    CancelToken RegisterCallback(std::function<void()> cb) const;

    /** @brief 注销回调（未使用，保留以便扩展）。 */
    void UnregisterCallback(CancelToken token) const;

protected:
    ContextPtr     parent_;
    CancelStatePtr state_;
};

// ============================================================================
// 3. 超时上下文
// ============================================================================
class TimerContext : public CancelContext
{
public:
    TimerContext();
    ~TimerContext() override = default;

    /**
         * @brief 初始化超时上下文。
         * @param parent 父上下文。
         * @param deadline 绝对截止时间。
         *
         * TODO: 需向全局时间轮注册一个到期回调，到期时调用 this->Cancel(ContextError::kDeadlineExceeded)。
         *       建议使用 weak_from_this() 防止循环引用，并在取消/Reset 时注销定时器。
         *       注册示例：
         *         auto& wheel = GetGlobalTimingWheel();
         *         timer_id_ = wheel.AddTaskAt(deadline, [weak = weak_from_this()]() {
         *             if (auto ctx = weak.lock()) ctx->Cancel(ContextError::kDeadlineExceeded);
         *         });
         */
    void Init(ContextPtr parent, std::chrono::system_clock::time_point deadline);

    /**
        * @brief 激活时间轮，注册到期回调。
        *
        * 调用后上下文将在截止时间到达时自动调用 Cancel(ContextError::kDeadlineExceeded)。
        * 重复调用此函数不会有副作用；若上下文已结束则不会注册。
        */
    void ActivateTimer();

    void                                                               Reset() override;
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override;

private:
    std::chrono::system_clock::time_point deadline_;
    std::atomic<uint64_t>                 timer_id_{0};
    std::atomic<bool>                     timer_activated_{false};
};

// ============================================================================
// 4. 值上下文
// ============================================================================
class ValueContext : public Context
{
public:
    ValueContext();

    /**
         * @brief 设置父上下文与键值对。
         * @param parent 父上下文。
         * @param key 键（指针比较）。
         * @param value 值。
         */
    void Set(ContextPtr parent, const void *key, std::any value);

    void                                                               Reset() override;
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override;
    [[nodiscard]] bool                                                 IsDone() const override;
    [[nodiscard]] ContextError                                         Err() const override;
    void                                                               Wait() const override;
    std::any                                                           Value(const void *key) const override;

private:
    ContextPtr  parent_;
    const void *key_ = nullptr;
    std::any    value_;
};

// ============================================================================
// Context 对象池及工厂函数
// ============================================================================
class ContextPool
{
public:
    static std::shared_ptr<EmptyContext> Background();
    static PooledPtr<CancelContext>      AcquireCancel();
    static PooledPtr<TimerContext>       AcquireTimer();
    static PooledPtr<ValueContext>       AcquireValue();

private:
    inline static ObjectPool<CancelContext> cancel_pool_;
    inline static ObjectPool<TimerContext>  timer_pool_;
    inline static ObjectPool<ValueContext>  value_pool_;
};

// ------------------------------ 便捷工厂函数 ------------------------------

/**
     * @brief 创建带有取消功能的新上下文。
     * @param parent 父上下文。
     * @return 新的 ContextPtr，其底层由对象池管理。
     */
ContextPtr WithCancel(ContextPtr parent);

/**
     * @brief 创建带有绝对截止时间的上下文。
     * @param parent 父上下文。
     * @param deadline 截止时间点（系统时钟）。
     * @return 新的 ContextPtr。
     */
ContextPtr WithDeadline(ContextPtr parent, std::chrono::system_clock::time_point deadline);

/**
     * @brief 创建带有相对超时的上下文。
     * @param parent 父上下文。
     * @param timeout 超时毫秒数。
     * @return 新的 ContextPtr。
     */
ContextPtr WithTimeout(ContextPtr parent, std::chrono::milliseconds timeout);

/**
     * @brief 创建带键值的上下文。
     * @param parent 父上下文。
     * @param key 键（任意指针）。
     * @param value 值。
     * @return 新的 ContextPtr。
     */
ContextPtr WithValue(ContextPtr parent, const void *key, std::any value);
}// namespace tyke
