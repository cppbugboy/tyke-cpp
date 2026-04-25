#include "component/context.h"

// 时间轮和线程池的头文件仅用于 TODO 注释中的集成说明，实际引入需按需取消注释。
#include "common/log_def.h"
#include "component/thread_pool.h"
#include "component/timing_wheel.h"

namespace tyke
{
// ============================================================================
// EmptyContext
// ============================================================================
std::optional<std::chrono::system_clock::time_point> EmptyContext::Deadline() const
{ return std::nullopt; }

bool EmptyContext::IsDone() const
{ return false; }

ContextError EmptyContext::Err() const
{ return ContextError::kNone; }

void EmptyContext::Wait() const
{
}

std::any EmptyContext::Value(const void * /*key*/) const
{ return {}; }

void EmptyContext::Reset()
{
}

// ============================================================================
// CancelContext
// ============================================================================
CancelContext::CancelContext() : state_(std::make_shared<CancelState>())
{
}

void CancelContext::Init(ContextPtr parent)
{
    parent_ = std::move(parent);
    if (parent_ && parent_->IsDone())
    {
        Cancel(parent_->Err());
    }
}

void CancelContext::Reset()
{
    state_->Reset();
    parent_.reset();
}

std::optional<std::chrono::system_clock::time_point> CancelContext::Deadline() const
{ return parent_ ? parent_->Deadline() : std::nullopt; }

bool CancelContext::IsDone() const
{ return state_->atomic_done.load(std::memory_order_acquire); }

ContextError CancelContext::Err() const
{
    std::lock_guard<std::mutex> lock(state_->mu);
    return state_->err;
}

void CancelContext::Wait() const
{
    if (IsDone())
        return;
    std::unique_lock<std::mutex> lock(state_->mu);
    state_->cv.wait(lock, [this] { return state_->atomic_done.load(std::memory_order_acquire); });
}

std::any CancelContext::Value(const void *key) const
{ return parent_ ? parent_->Value(key) : std::any(); }

void CancelContext::Cancel(const ContextError err) const
{
    std::unordered_map<CancelToken, std::function<void()>> cbs;
    {
        std::lock_guard<std::mutex> lock(state_->mu);
        if (state_->err != ContextError::kNone)
            return;// 避免重复取消
        state_->err = err;
        state_->atomic_done.store(true, std::memory_order_release);
        cbs = std::move(state_->callbacks);
    }
    state_->cv.notify_all();

    /**
         * TODO: 将回调提交到全局线程池异步执行，避免阻塞调用线程，并防止死锁。
         * 典型实现：
         *   auto& pool = GetGlobalThreadPool();
         *   for (auto& [token, cb] : cbs) {
         *     if (cb) pool.Enqueue(std::move(cb));
         *   }
         * 注意：如果回调本身需要访问 Context 或其父节点，请确保生命周期安全。
         */
    for (auto &[token, cb]: cbs)
    {
        if (cb)
        {
            GetGlobalThreadPool().Enqueue(cb);
        }
    }
}

CancelToken CancelContext::RegisterCallback(std::function<void()> cb) const
{
    std::lock_guard<std::mutex> lock(state_->mu);
    CancelToken                 token = state_->next_token.fetch_add(1, std::memory_order_relaxed);
    state_->callbacks.emplace(token, std::move(cb));
    return token;
}

void CancelContext::UnregisterCallback(const CancelToken token) const
{
    std::lock_guard<std::mutex> lock(state_->mu);
    state_->callbacks.erase(token);
}

// ============================================================================
// TimerContext
// ============================================================================
TimerContext::TimerContext() = default;

void TimerContext::Init(ContextPtr parent, const std::chrono::system_clock::time_point deadline)
{
    CancelContext::Init(std::move(parent));

    // 与父上下文的截止时间比较，取较早者
    auto effective_deadline = deadline;
    if (parent_ && parent_->Deadline().has_value())
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

void TimerContext::ActivateTimer()
{
    // 防止重复激活
    if (timer_activated_.load(std::memory_order_acquire))
        return;

    if (IsDone())
        return;

    std::weak_ptr<TimerContext> weak;
    try
    {
        weak = std::weak_ptr<TimerContext>(std::static_pointer_cast<TimerContext>(shared_from_this()));
    }
    catch (const std::bad_weak_ptr &)
    {
        LOG_ERROR("TimerContext::ActivateTimer called without shared_ptr management");
        return;
    }

    // 将系统时钟截止时间转换为稳态时钟
    const auto steady_now      = std::chrono::steady_clock::now();
    const auto sys_now         = std::chrono::system_clock::now();
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

std::optional<std::chrono::system_clock::time_point> TimerContext::Deadline() const
{ return deadline_; }

// ============================================================================
// ValueContext
// ============================================================================
ValueContext::ValueContext() = default;

void ValueContext::Set(ContextPtr parent, const void *key, std::any value)
{
    parent_ = std::move(parent);
    key_    = key;
    value_  = std::move(value);
}

void ValueContext::Reset()
{
    parent_.reset();
    key_ = nullptr;
    value_.reset();
}

std::optional<std::chrono::system_clock::time_point> ValueContext::Deadline() const
{ return parent_ ? parent_->Deadline() : std::nullopt; }

bool ValueContext::IsDone() const
{ return parent_ ? parent_->IsDone() : false; }

ContextError ValueContext::Err() const
{ return parent_ ? parent_->Err() : ContextError::kNone; }

void ValueContext::Wait() const
{
    if (parent_)
        parent_->Wait();
}

std::any ValueContext::Value(const void *key) const
{
    if (key == key_)
        return value_;
    return parent_ ? parent_->Value(key) : std::any();
}

// ============================================================================
// ContextPool & Factory Functions
// ============================================================================
std::shared_ptr<EmptyContext> ContextPool::Background()
{
    static auto instance = std::make_shared<class EmptyContext>();
    return instance;
}

PooledPtr<CancelContext> ContextPool::AcquireCancel()
{ return PooledPtr<CancelContext>::Acquire(cancel_pool_); }

PooledPtr<TimerContext> ContextPool::AcquireTimer()
{ return PooledPtr<TimerContext>::Acquire(timer_pool_); }

PooledPtr<ValueContext> ContextPool::AcquireValue()
{ return PooledPtr<ValueContext>::Acquire(value_pool_); }

ContextPtr WithCancel(ContextPtr parent)
{
    auto  pooled = ContextPool::AcquireCancel();
    auto *raw    = pooled.Get();
    raw->Init(std::move(parent));
    return std::shared_ptr<CancelContext>(raw,
                                          [pooled = std::move(pooled)](CancelContext *p) mutable
                                          {
                                              p->Reset();    // 先重置对象状态
                                              pooled.Reset();// 再归还到对象池
                                          });
}

ContextPtr WithDeadline(ContextPtr parent, const std::chrono::system_clock::time_point deadline)
{
    auto  pooled = ContextPool::AcquireTimer();
    auto *raw    = pooled.Get();
    raw->Init(std::move(parent), deadline);
    return std::shared_ptr<TimerContext>(raw,
                                         [pooled = std::move(pooled)](TimerContext *p) mutable
                                         {
                                             p->Reset();
                                             pooled.Reset();
                                         });
}

ContextPtr WithTimeout(ContextPtr parent, const std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::system_clock::now() + timeout;
    return WithDeadline(std::move(parent), deadline);
}

ContextPtr WithValue(ContextPtr parent, const void *key, std::any value)
{
    auto  pooled = ContextPool::AcquireValue();
    auto *raw    = pooled.Get();
    raw->Set(std::move(parent), key, std::move(value));
    return std::shared_ptr<ValueContext>(raw,
                                         [pooled = std::move(pooled)](ValueContext *p) mutable
                                         {
                                             p->Reset();
                                             pooled.Reset();
                                         });
}
}// namespace tyke
