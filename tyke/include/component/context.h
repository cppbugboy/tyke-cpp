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
#include <utility>

namespace tyke
{
    enum class ContextError
    {
        kNone = 0,
        kCanceled,
        kDeadlineExceeded
    };

    class Context;
    using ContextPtr = std::shared_ptr<Context>;

    using CancelToken = uint64_t;
    constexpr CancelToken kInvalidToken = 0;

    struct CancelState
    {
        mutable std::mutex mu;
        mutable std::condition_variable cv;
        std::atomic<bool> atomic_done{false};
        ContextError err{ContextError::kNone};
        CancelToken next_token{1};
        std::unordered_map<CancelToken, std::function<void()>> callbacks;

        void Reset()
        {
            std::lock_guard<std::mutex> lock(mu);
            atomic_done = false;
            err = ContextError::kNone;
            next_token = 1;
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
        virtual std::optional<std::chrono::system_clock::time_point> Deadline() const = 0;
        virtual bool IsDone() const = 0;
        virtual ContextError Err() const = 0;
        virtual void Wait() const = 0;
        virtual std::any Value(const void* key) const = 0;

        // 为对象池新增的接口
        virtual void Reset() = 0;
    };

    // ============================================================================
    // 1. EmptyContext (Background/TODO)
    // ============================================================================
    class EmptyContext : public Context
    {
    public:
        [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override { return std::nullopt; }
        [[nodiscard]] bool IsDone() const override { return false; }
        [[nodiscard]] ContextError Err() const override { return ContextError::kNone; }

        void Wait() const override
        {
        }

        std::any Value(const void* /*key*/) const override { return {}; }

        void Reset() override
        {
        } // 单例对象无需重置
    };

    // ============================================================================
    // 2. CancelContext
    // ============================================================================
    class CancelContext : public Context, public std::enable_shared_from_this<CancelContext>
    {
    public:
        CancelContext() : state_(std::make_shared<CancelState>())
        {
        }

        // 对象池初始化入口
        void Init(ContextPtr parent)
        {
            parent_ = std::move(parent);
            if (parent_ && parent_->IsDone())
            {
                Cancel(parent_->Err());
            }
        }

        void Reset() override
        {
            state_->Reset();
            parent_.reset();
        }

        std::optional<std::chrono::system_clock::time_point> Deadline() const override
        {
            return parent_ ? parent_->Deadline() : std::nullopt;
        }

        bool IsDone() const override { return state_->atomic_done.load(); }

        ContextError Err() const override
        {
            std::lock_guard<std::mutex> lock(state_->mu);
            return state_->err;
        }

        void Wait() const override
        {
            if (IsDone()) return;
            std::unique_lock<std::mutex> lock(state_->mu);
            state_->cv.wait(lock, [this] { return state_->atomic_done.load(); });
        }

        std::any Value(const void* key) const override { return parent_ ? parent_->Value(key) : std::any(); }

        void Cancel(const ContextError err) const
        {
            std::unordered_map<CancelToken, std::function<void()>> cbs;
            {
                std::lock_guard<std::mutex> lock(state_->mu);
                if (state_->err != ContextError::kNone) return;
                state_->err = err;
                state_->atomic_done = true;
                cbs = std::move(state_->callbacks);
            }
            state_->cv.notify_all();
            for (auto& [fst, snd] : cbs) { snd(); }
        }

    protected:
        ContextPtr parent_;
        CancelStatePtr state_;
    };

    // ============================================================================
    // 3. TimerContext
    // ============================================================================
    class TimerContext : public CancelContext
    {
    public:
        TimerContext() = default;

        void Init(ContextPtr parent, const std::chrono::system_clock::time_point deadline)
        {
            CancelContext::Init(std::move(parent));
            deadline_ = deadline;
            // 注意：实际生产中这里应启动定时器线程或注册到全局 TimerWheel
        }

        void Reset() override
        {
            // 此处应关闭定时器
            deadline_ = {};
            CancelContext::Reset();
        }

        std::optional<std::chrono::system_clock::time_point> Deadline() const override { return deadline_; }

    private:
        std::chrono::system_clock::time_point deadline_;
    };

    // ============================================================================
    // 4. ValueContext
    // ============================================================================
    class ValueContext : public Context
    {
    public:
        ValueContext() = default;

        void Set(ContextPtr parent, const void* key, std::any value)
        {
            parent_ = std::move(parent);
            key_ = key;
            value_ = std::move(value);
        }

        void Reset() override
        {
            parent_.reset();
            key_ = nullptr;
            value_.reset();
        }

        [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override
        {
            return parent_ ? parent_->Deadline() : std::nullopt;
        }

        [[nodiscard]] bool IsDone() const override { return parent_ ? parent_->IsDone() : false; }
        [[nodiscard]] ContextError Err() const override { return parent_ ? parent_->Err() : ContextError::kNone; }
        void Wait() const override { if (parent_) parent_->Wait(); }

        std::any Value(const void* key) const override
        {
            if (key == key_) return value_;
            return parent_ ? parent_->Value(key) : std::any();
        }

    private:
        ContextPtr parent_;
        const void* key_ = nullptr;
        std::any value_;
    };
}
