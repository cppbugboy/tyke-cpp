/**
 * @file context.cpp
 * @brief 上下文体系实现：取消、超时、值传递。
 *
 * 本文件实现了上下文系统的所有类，包括：
 * - EmptyContext: 空上下文（Background）
 * - CancelContext: 可取消上下文
 * - TimerContext: 超时上下文
 * - ValueContext: 值上下文
 * - ContextPool: 上下文对象池
 * - 工厂函数：WithCancel, WithDeadline, WithTimeout, WithValue
 *
 * @see context.h
 * @author Nick
 * @date 2026/04/26
 */

#include "component/context.h"

#include "common/log_def.h"
#include "component/thread_pool.h"
#include "component/timing_wheel.h"

namespace tyke
{
    // ============================================================================
    // EmptyContext 实现
    // ============================================================================

    /**
     * @brief 获取截止时间
     *
     * 空上下文没有截止时间。
     *
     * @return std::nullopt 始终返回空
     */
    std::optional<std::chrono::system_clock::time_point> EmptyContext::Deadline() const
    {
        return std::nullopt;
    }

    /**
     * @brief 检查是否已完成
     *
     * 空上下文永远不会取消。
     *
     * @return false 始终返回false
     */
    bool EmptyContext::IsDone() const
    {
        return false;
    }

    /**
     * @brief 获取取消原因
     *
     * @return ContextError::kNone 始终返回无错误
     */
    ContextError EmptyContext::Err() const
    {
        return ContextError::kNone;
    }

    /**
     * @brief 等待完成
     *
     * 空上下文永远不会完成，此方法立即返回。
     */
    void EmptyContext::Wait() const
    {
    }

    /**
     * @brief 获取值
     *
     * 空上下文没有值。
     *
     * @return 空的std::any
     */
    std::any EmptyContext::Value(const void* /*key*/) const
    {
        return {};
    }

    /**
     * @brief 重置
     *
     * 空上下文无需重置。
     */
    void EmptyContext::Reset()
    {
    }

    // ============================================================================
    // CancelContext 实现
    // ============================================================================

    /**
     * @brief 构造函数
     *
     * 初始化取消状态。
     */
    CancelContext::CancelContext() : state_(std::make_shared<CancelState>())
    {
    }

    /**
     * @brief 初始化
     *
     * 设置父上下文，如果父上下文已取消则立即传播取消状态。
     *
     * @param parent 父上下文
     */
    void CancelContext::Init(ContextPtr parent)
    {
        parent_ = std::move(parent);
        if (parent_&& parent_
        ->
        IsDone()
        )
        {
            Cancel(parent_->Err());
        }
    }

    /**
     * @brief 重置
     *
     * 清除取消状态和父上下文，供对象池复用。
     */
    void CancelContext::Reset()
    {
        state_->Reset();
        parent_.reset();
    }

    /**
     * @brief 获取截止时间
     *
     * 委托给父上下文。
     *
     * @return 父上下文的截止时间，或nullopt
     */
    std::optional<std::chrono::system_clock::time_point> CancelContext::Deadline() const
    {
        return parent_ ? parent_->Deadline() : std::nullopt;
    }

    /**
     * @brief 检查是否已完成
     *
     * @return true 已取消
     * @return false 未取消
     */
    bool CancelContext::IsDone() const
    {
        return state_->atomic_done.load(std::memory_order_acquire);
    }

    /**
     * @brief 获取取消原因
     *
     * @return ContextError 取消原因
     */
    ContextError CancelContext::Err() const
    {
        std::lock_guard<std::mutex> lock(state_->mu);
        return state_->err;
    }

    /**
     * @brief 等待完成
     *
     * 阻塞直到上下文被取消。
     */
    void CancelContext::Wait() const
    {
        if (IsDone())
            return;
        std::unique_lock<std::mutex> lock(state_->mu);
        state_->cv.wait(lock, [this] { return state_->atomic_done.load(std::memory_order_acquire); });
    }

    /**
     * @brief 获取值
     *
     * 委托给父上下文。
     *
     * @param key 键
     * @return 值
     */
    std::any CancelContext::Value(const void* key) const
    {
        return parent_ ? parent_->Value(key) : std::any();
    }

    /**
     * @brief 触发取消
     *
     * 设置取消原因，通知所有等待者，并异步调用所有已注册的回调。
     * 回调被提交到全局线程池执行。
     *
     * @param err 取消原因
     */
    void CancelContext::Cancel(const ContextError err) const
    {
        std::unordered_map<CancelToken, std::function<void()>> cbs;
        {
            std::lock_guard<std::mutex> lock(state_->mu);
            if (state_->err != ContextError::kNone)
                return; // 避免重复取消
            state_->err = err;
            state_->atomic_done.store(true, std::memory_order_release);
            cbs = std::move(state_->callbacks);
        }
        state_->cv.notify_all();

        // 将回调提交到全局线程池异步执行
        for (auto& [token, cb] : cbs)
        {
            if (cb)
            {
                GetGlobalThreadPool().Enqueue(cb);
            }
        }
    }

    /**
     * @brief 注册取消回调
     *
     * @param cb 回调函数
     * @return CancelToken 令牌，用于注销
     */
    CancelToken CancelContext::RegisterCallback(std::function<void()> cb) const
    {
        std::lock_guard<std::mutex> lock(state_->mu);
        CancelToken token = state_->next_token.fetch_add(1, std::memory_order_relaxed);
        state_->callbacks.emplace(token, std::move(cb));
        return token;
    }

    /**
     * @brief 注销回调
     *
     * @param token 令牌
     */
    void CancelContext::UnregisterCallback(const CancelToken token) const
    {
        std::lock_guard<std::mutex> lock(state_->mu);
        state_->callbacks.erase(token);
    }

    // ============================================================================
    // TimerContext 实现
    // ============================================================================

    /**
     * @brief 构造函数
     */
    TimerContext::TimerContext() = default;

    /**
     * @brief 初始化超时上下文
     *
     * 设置截止时间，与父上下文的截止时间比较取较早者。
     *
     * @param parent 父上下文
     * @param deadline 截止时间
     */
    void TimerContext::Init(ContextPtr parent, const std::chrono::system_clock::time_point deadline)
    {
        CancelContext::Init(std::move(parent));

        // 与父上下文的截止时间比较，取较早者
        auto effective_deadline = deadline;
        if (parent_&& parent_
        ->
        Deadline().has_value()
        )
        {
            if (const auto parent_deadline = parent_->Deadline().value(); parent_deadline < effective_deadline)
            {
                effective_deadline = parent_deadline;
            }
        }
        deadline_ = effective_deadline;
        timer_activated_.store(false, std::memory_order_relaxed);
        timer_id_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief 激活定时器
     *
     * 向全局时间轮注册到期回调，到期时自动调用Cancel。
     * 使用weak_ptr防止循环引用。
     */
    void TimerContext::ActivateTimer()
    {
        // 防止重复激活
        if (timer_activated_.load(std::memory_order_acquire))
            return;

        if (IsDone())
            return;

        std::weak_ptr < TimerContext > weak;
        try
        {
            weak = std::weak_ptr < TimerContext > (std::static_pointer_cast < TimerContext > (shared_from_this()));
        }
        catch (const std::bad_weak_ptr&)
        {
            LOG_ERROR("TimerContext::ActivateTimer called without shared_ptr management");
            return;
        }

        // 将系统时钟截止时间转换为稳态时钟
        const auto steady_now = std::chrono::steady_clock::now();
        const auto sys_now = std::chrono::system_clock::now();
        const auto steady_deadline = steady_now + (deadline_ - sys_now);

        // 向全局时间轮注册到期回调
        timer_id_.store(GetGlobalTimingWheel().AddTaskAt(steady_deadline,
                                                         [weak = std::move(weak)]()
                                                         {
                                                             if (const auto ctx = weak.lock())
                                                             {
                                                                 ctx->Cancel(ContextError::kDeadlineExceeded);
                                                             }
                                                         }),
                        std::memory_order_release);

        timer_activated_.store(true, std::memory_order_release);
    }

    /**
     * @brief 重置
     *
     * 取消定时器并重置所有状态。
     */
    void TimerContext::Reset()
    {
        if (const auto id = timer_id_.load(std::memory_order_acquire); id != 0)
        {
            GetGlobalTimingWheel().CancelTask(id);
            timer_id_.store(0, std::memory_order_release);
        }
        timer_activated_.store(false, std::memory_order_release);
        deadline_ = {};
        CancelContext::Reset();
    }

    /**
     * @brief 获取截止时间
     *
     * @return 截止时间
     */
    std::optional<std::chrono::system_clock::time_point> TimerContext::Deadline() const
    {
        return deadline_;
    }

    // ============================================================================
    // ValueContext 实现
    // ============================================================================

    /**
     * @brief 构造函数
     */
    ValueContext::ValueContext() = default;

    /**
     * @brief 设置父上下文和键值对
     *
     * @param parent 父上下文
     * @param key 键
     * @param value 值
     */
    void ValueContext::Set(ContextPtr parent, const void* key, std::any value)
    {
        parent_ = std::move(parent);
        key_ = key;
        value_ = std::move(value);
    }

    /**
     * @brief 重置
     *
     * 清除所有状态，供对象池复用。
     */
    void ValueContext::Reset()
    {
        parent_.reset();
        key_ = nullptr;
        value_.reset();
    }

    /**
     * @brief 获取截止时间
     *
     * 委托给父上下文。
     */
    std::optional<std::chrono::system_clock::time_point> ValueContext::Deadline() const
    {
        return parent_ ? parent_->Deadline() : std::nullopt;
    }

    /**
     * @brief 检查是否已完成
     *
     * 委托给父上下文。
     */
    bool ValueContext::IsDone() const
    {
        return parent_ ? parent_->IsDone() : false;
    }

    /**
     * @brief 获取取消原因
     *
     * 委托给父上下文。
     */
    ContextError ValueContext::Err() const
    {
        return parent_ ? parent_->Err() : ContextError::kNone;
    }

    /**
     * @brief 等待完成
     *
     * 委托给父上下文。
     */
    void ValueContext::Wait() const
    {
        if (parent_)
            parent_->Wait();
    }

    /**
     * @brief 获取值
     *
     * 如果键匹配则返回本地值，否则委托给父上下文。
     *
     * @param key 键
     * @return 值
     */
    std::any ValueContext::Value(const void* key) const
    {
        if (key == key_)
            return value_;
        return parent_ ? parent_->Value(key) : std::any();
    }

    // ============================================================================
    // ContextPool 实现
    // ============================================================================

    /**
     * @brief 获取空上下文（Background）
     *
     * @return 空上下文单例
     */
    std::shared_ptr<EmptyContext> ContextPool::Background()
    {
        static auto instance = std::make_shared<class EmptyContext>();
        return instance;
    }

    /**
     * @brief 从池中获取CancelContext
     *
     * @return 池化的可取消上下文
     */
    PooledPtr<CancelContext> ContextPool::AcquireCancel()
    {
        return PooledPtr < CancelContext > ::Acquire(cancel_pool_);
    }

    /**
     * @brief 从池中获取TimerContext
     *
     * @return 池化的定时器上下文
     */
    PooledPtr<TimerContext> ContextPool::AcquireTimer()
    {
        return PooledPtr < TimerContext > ::Acquire(timer_pool_);
    }

    /**
     * @brief 从池中获取ValueContext
     *
     * @return 池化的值上下文
     */
    PooledPtr<ValueContext> ContextPool::AcquireValue()
    {
        return PooledPtr < ValueContext > ::Acquire(value_pool_);
    }

    // ============================================================================
    // 工厂函数实现
    // ============================================================================

    /**
     * @brief 创建带有取消功能的新上下文
     *
     * @param parent 父上下文
     * @return 新的可取消上下文
     */
    ContextPtr WithCancel(ContextPtr parent)
    {
        auto pooled = ContextPool::AcquireCancel();
        auto* raw = pooled.Get();
        raw->Init(std::move(parent));
        return std::shared_ptr < CancelContext > (raw,
            [pooled = std::move(pooled)](CancelContext* p) mutable
            {
                p->Reset(); // 先重置对象状态
                pooled.Reset(); // 再归还到对象池
            });
    }

    /**
     * @brief 创建带有绝对截止时间的上下文
     *
     * @param parent 父上下文
     * @param deadline 截止时间
     * @return 新的截止时间上下文
     */
    ContextPtr WithDeadline(ContextPtr parent, const std::chrono::system_clock::time_point deadline)
    {
        auto pooled = ContextPool::AcquireTimer();
        auto* raw = pooled.Get();
        raw->Init(std::move(parent), deadline);
        return std::shared_ptr < TimerContext > (raw,
            [pooled = std::move(pooled)](TimerContext* p) mutable
            {
                p->Reset();
                pooled.Reset();
            });
    }

    /**
     * @brief 创建带有相对超时的上下文
     *
     * @param parent 父上下文
     * @param timeout 超时时间
     * @return 新的超时上下文
     */
    ContextPtr WithTimeout(ContextPtr parent, const std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::system_clock::now() + timeout;
        return WithDeadline(std::move(parent), deadline);
    }

    /**
     * @brief 创建带键值的上下文
     *
     * @param parent 父上下文
     * @param key 键
     * @param value 值
     * @return 新的值上下文
     */
    ContextPtr WithValue(ContextPtr parent, const void* key, std::any value)
    {
        auto pooled = ContextPool::AcquireValue();
        auto* raw = pooled.Get();
        raw->Set(std::move(parent), key, std::move(value));
        return std::shared_ptr < ValueContext > (raw,
            [pooled = std::move(pooled)](ValueContext* p) mutable
            {
                p->Reset();
                pooled.Reset();
            });
    }
} // namespace tyke