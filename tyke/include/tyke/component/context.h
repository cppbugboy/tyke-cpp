/**
 * @file context.h
 * @brief 上下文体系：取消、超时、值传递。
 *
 * 本文件实现了一个类似Go语言context包的上下文系统，
 * 支持取消传播、截止时间和键值携带。
 * 通过 ContextPool 实现对象复用，降低高频创建开销。
 *
 * @section features 主要特性
 * - CancelContext: 手动取消，支持回调注册
 * - TimerContext: 在截止时间/超时后自动取消
 * - ValueContext: 键值对传播
 * - 所有上下文类型都使用对象池
 * - 线程安全操作
 *
 * @section usage 使用示例
 * @code
 * // 创建可取消的上下文
 * auto ctx = tyke::WithCancel(nullptr);
 *
 * // 创建超时上下文
 * auto ctx = tyke::WithTimeout(nullptr, std::chrono::seconds(5));
 *
 * // 检查是否完成
 * if (ctx->IsDone()) {
 *     auto err = ctx->Err();
 * }
 *
 * // 等待取消
 * ctx->Wait();
 * @endcode
 *
 * @author Nick
 * @date 2026/04/26
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
    /**
     * @brief 上下文错误码枚举
     *
     * 表示上下文被取消的原因。
     */
    enum class ContextError
    {
        kNone = 0, ///< 上下文仍然活跃
        kCanceled, ///< 上下文被手动取消
        kDeadlineExceeded, ///< 上下文截止时间已过
    };

    /**
     * @brief 前置声明
     */
    class Context;
    using ContextPtr = std::shared_ptr<Context>; ///< 上下文指针类型

    using CancelToken = uint64_t; ///< 取消回调令牌类型
    constexpr CancelToken kInvalidCancelToken = 0; ///< 无效取消令牌

    /**
     * @brief 取消状态共享结构
     *
     * 保存取消管理的内部状态，包括条件变量、错误码和回调列表。
     */
    struct CancelState
    {
        mutable std::mutex mu; ///< 保护所有字段的互斥锁
        mutable std::condition_variable cv; ///< 用于Wait()的条件变量
        std::atomic<bool> atomic_done{false}; ///< 完成状态的快速检查
        ContextError err{ContextError::kNone}; ///< 取消原因
        std::atomic<CancelToken> next_token{1}; ///< 下一个回调令牌
        std::unordered_map<CancelToken, std::function<void()>> callbacks; ///< 已注册的回调

        /**
         * @brief 重置取消状态
         *
         * 清除所有状态，供对象池复用。
         */
        void Reset()
        {
            std::lock_guard<std::mutex> lock(mu);
            atomic_done = false;
            err = ContextError::kNone;
            next_token = 1;
            callbacks.clear();
        }
    };

    using CancelStatePtr = std::shared_ptr<CancelState>; ///< 取消状态指针类型

    /**
     * @brief 上下文核心接口
     *
     * 定义所有上下文类型的公共接口，包括截止时间检查、取消、等待和值检索。
     */
    class Context
    {
    public:
        virtual ~Context() = default;

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;

        /**
         * @brief 获取截止时间
         *
         * @return std::optional<std::chrono::system_clock::time_point>
         *         截止时间，如果没有设置返回nullopt
         */
        [[nodiscard]] virtual std::optional<std::chrono::system_clock::time_point> Deadline() const = 0;

        /**
         * @brief 检查上下文是否已完成
         *
         * @return true 上下文已被取消
         * @return false 上下文仍然活跃
         */
        [[nodiscard]] virtual bool IsDone() const = 0;

        /**
         * @brief 获取取消原因
         *
         * @return ContextError 取消原因，未取消返回kNone
         */
        [[nodiscard]] virtual ContextError Err() const = 0;

        /**
         * @brief 阻塞等待上下文完成
         */
        virtual void Wait() const = 0;

        /**
         * @brief 检索与给定键关联的值
         *
         * @param key 键指针
         * @return std::any 值，不存在返回空any
         */
        virtual std::any Value(const void* key) const = 0;

        /**
         * @brief 重置对象状态，供对象池使用
         */
        virtual void Reset() = 0;

    protected:
        Context() = default;
    };

    /**
     * @brief 空上下文
     *
     * 一个永远不会取消、没有截止时间或值的上下文。
     * 类似于Go语言中的context.Background()。
     */
    class EmptyContext final : public Context
    {
    public:
        [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override;
        [[nodiscard]] bool IsDone() const override;
        [[nodiscard]] ContextError Err() const override;
        void Wait() const override;
        std::any Value(const void* key) const override;
        void Reset() override;
    };

    /**
     * @brief 可取消上下文
     *
     * 一个可以手动取消的上下文，支持注册在取消时调用的回调函数。
     */
    class CancelContext : public Context, public std::enable_shared_from_this<CancelContext>
    {
    public:
        /**
         * @brief 构造函数
         */
        CancelContext();

        ~CancelContext() override = default;

        /**
         * @brief 从对象池初始化
         *
         * @param parent 父上下文，可为nullptr
         *
         * 如果父上下文已结束，则立即传播取消状态。
         */
        void Init(ContextPtr parent);

        void Reset() override;

        [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override;
        [[nodiscard]] bool IsDone() const override;
        [[nodiscard]] ContextError Err() const override;
        void Wait() const override;
        std::any Value(const void* key) const override;

        /**
         * @brief 触发取消
         *
         * 设置取消原因，并调用所有已注册的回调。
         * 回调在新的线程中异步执行。
         *
         * @param err 取消原因
         */
        void Cancel(ContextError err) const;

        /**
         * @brief 注册取消回调
         *
         * @param cb 回调函数
         * @return CancelToken 令牌，可用于注销
         */
        CancelToken RegisterCallback(std::function<void()> cb) const;

        /**
         * @brief 注销回调
         *
         * @param token 从RegisterCallback返回的令牌
         */
        void UnregisterCallback(CancelToken token) const;

    protected:
        ContextPtr parent_; ///< 父上下文
        CancelStatePtr state_; ///< 取消状态
    };

    /**
     * @brief 超时上下文
     *
     * 一个在截止时间自动取消的上下文，继承自CancelContext。
     */
    class TimerContext : public CancelContext
    {
    public:
        /**
         * @brief 构造函数
         */
        TimerContext();

        ~TimerContext() override = default;

        /**
         * @brief 初始化超时上下文
         *
         * @param parent 父上下文
         * @param deadline 绝对截止时间
         *
         * 有效截止时间是给定截止时间和父上下文截止时间中较早的一个。
         */
        void Init(ContextPtr parent, std::chrono::system_clock::time_point deadline);

        /**
         * @brief 激活定时器
         *
         * 调用后上下文将在截止时间到达时自动调用Cancel(ContextError::kDeadlineExceeded)。
         * 重复调用此函数不会有副作用；若上下文已结束则不会注册。
         */
        void ActivateTimer();

        void Reset() override;
        [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override;

    private:
        std::chrono::system_clock::time_point deadline_; ///< 截止时间
        std::atomic<uint64_t> timer_id_{0}; ///< 时间轮定时器ID
        std::atomic<bool> timer_activated_{false}; ///< 定时器是否已激活
    };

    /**
     * @brief 值上下文
     *
     * 一个携带单个键值对的上下文，将所有其他操作委托给父上下文。
     */
    class ValueContext : public Context
    {
    public:
        /**
         * @brief 构造函数
         */
        ValueContext();

        /**
         * @brief 设置父上下文和键值对
         *
         * @param parent 父上下文
         * @param key 键（指针比较）
         * @param value 值
         */
        void Set(ContextPtr parent, const void* key, std::any value);

        void Reset() override;
        [[nodiscard]] std::optional<std::chrono::system_clock::time_point> Deadline() const override;
        [[nodiscard]] bool IsDone() const override;
        [[nodiscard]] ContextError Err() const override;
        void Wait() const override;
        std::any Value(const void* key) const override;

    private:
        ContextPtr parent_; ///< 父上下文
        const void* key_ = nullptr; ///< 键
        std::any value_; ///< 值
    };

    /**
     * @brief 上下文对象池
     *
     * 管理各种上下文类型的对象池，提供获取和释放接口。
     */
    class ContextPool
    {
    public:
        /**
         * @brief 获取空上下文（Background）
         *
         * @return std::shared_ptr<EmptyContext> 空上下文
         */
        static std::shared_ptr<EmptyContext> Background();

        /**
         * @brief 从池中获取CancelContext
         *
         * @return PooledPtr<CancelContext> 池化的可取消上下文
         */
        static PooledPtr<CancelContext> AcquireCancel();

        /**
         * @brief 从池中获取TimerContext
         *
         * @return PooledPtr<TimerContext> 池化的定时器上下文
         */
        static PooledPtr<TimerContext> AcquireTimer();

        /**
         * @brief 从池中获取ValueContext
         *
         * @return PooledPtr<ValueContext> 池化的值上下文
         */
        static PooledPtr<ValueContext> AcquireValue();

    private:
        inline static ObjectPool<CancelContext> cancel_pool_; ///< CancelContext对象池
        inline static ObjectPool<TimerContext> timer_pool_; ///< TimerContext对象池
        inline static ObjectPool<ValueContext> value_pool_; ///< ValueContext对象池
    };

    /**
     * @brief 创建带有取消功能的新上下文
     *
     * @param parent 父上下文，可为nullptr
     * @return ContextPtr 新的可取消上下文
     *
     * @code
     * auto [ctx, cancel] = tyke::WithCancel(parent);
     * // 使用ctx...
     * cancel(); // 取消上下文
     * @endcode
     */
    ContextPtr WithCancel(ContextPtr parent);

    /**
     * @brief 创建带有绝对截止时间的上下文
     *
     * @param parent 父上下文
     * @param deadline 截止时间点（系统时钟）
     * @return ContextPtr 新的截止时间上下文
     */
    ContextPtr WithDeadline(ContextPtr parent, std::chrono::system_clock::time_point deadline);

    /**
     * @brief 创建带有相对超时的上下文
     *
     * @param parent 父上下文
     * @param timeout 超时时间
     * @return ContextPtr 新的超时上下文
     *
     * @code
     * auto ctx = tyke::WithTimeout(parent, std::chrono::seconds(5));
     * @endcode
     */
    ContextPtr WithTimeout(ContextPtr parent, std::chrono::milliseconds timeout);

    /**
     * @brief 创建带键值的上下文
     *
     * @param parent 父上下文
     * @param key 键（任意指针）
     * @param value 值
     * @return ContextPtr 新的值上下文
     *
     * @code
     * static const char* kRequestID = "request_id";
     * auto ctx = tyke::WithValue(parent, kRequestID, std::string("req-123"));
     * @endcode
     */
    ContextPtr WithValue(ContextPtr parent, const void* key, std::any value);
} // namespace tyke
