/**
 * @file context_factory.cpp
 * @brief 上下文工厂实现。提供 Background()、WithCancel、WithDeadline、WithTimeout、WithValue 等工厂方法。
 *
 * 工厂方法管理对象池（CancelContext、TimerContext、ValueContext），使用自定义删除器归还对象。
 * 每个工厂方法返回 ContextPtr + cancel函数的 pair，或仅返回 ContextPtr。
 *
 * @author Nick
 * @date 2026/04/26
 */

#include "tyke/core/context_factory.h"

namespace tyke::context
{
    /** @brief 获取全局 Background 上下文（永不取消、无截止时间）。 */
    ContextPtr ContextFactory::Background()
    {
        return ContextPool::Background();
    }

    /** @brief 获取 TODO 占位上下文（当前等同于 Background）。 */
    ContextPtr ContextFactory::TODO()
    {
        return Background();
    }

    /** @brief 将 CancelContext 归还对象池（先 Reset 后 Release）。 */
    void ContextFactory::ReleaseCancel(CancelContext* ctx)
    {
        if (ctx)
        {
            ctx->Reset();
            cancel_pool_.Release(ctx);
        }
    }

    /** @brief 将 TimerContext 归还对象池（先 Reset 取消定时器后 Release）。 */
    void ContextFactory::ReleaseTimer(TimerContext* ctx)
    {
        if (ctx)
        {
            ctx->Reset();
            timer_pool_.Release(ctx);
        }
    }

    /** @brief 将 ValueContext 归还对象池（先 Reset 后 Release）。 */
    void ContextFactory::ReleaseValue(ValueContext* ctx)
    {
        if (ctx)
        {
            ctx->Reset();
            value_pool_.Release(ctx);
        }
    }

    /**
     * @brief 创建可取消上下文。
     *
     * 从 cancel_pool_ 获取 CancelContext，初始化后包装为 shared_ptr（自定义删除器归还池）。
     * 返回 cancel 函数，调用后触发 Cancel(ContextError::kCanceled)。
     *
     * @param parent 父上下文
     * @return pair<ContextPtr, cancel_func>
     */
    std::pair<ContextPtr, std::function<void()>> ContextFactory::WithCancel(const ContextPtr& parent)
    {
        CancelContext* raw = cancel_pool_.Acquire();

        auto ctx = std::shared_ptr<CancelContext>(raw, [](CancelContext* p) { ContextFactory::ReleaseCancel(p); });

        ctx->Init(parent);

        std::weak_ptr<CancelContext> weak_ctx = ctx;
        auto cancel_func = [weak_ctx]()
        {
            if (const auto s = weak_ctx.lock())
            {
                s->Cancel(ContextError::kCanceled);
            }
        };

        return {std::move(ctx), std::move(cancel_func)};
    }

    /**
     * @brief 创建带绝对截止时间的上下文。
     *
     * 从 timer_pool_ 获取 TimerContext，使用 system_clock 截止时间初始化。
     *
     * @param parent 父上下文
     * @param deadline 系统时钟截止时间点
     * @return pair<ContextPtr, cancel_func>
     */
    std::pair<ContextPtr, std::function<void()>>
    ContextFactory::WithDeadline(const ContextPtr& parent, const std::chrono::system_clock::time_point deadline)
    {
        TimerContext* raw = timer_pool_.Acquire();

        auto ctx = std::shared_ptr<TimerContext>(raw, [](TimerContext* p) { ContextFactory::ReleaseTimer(p); });

        ctx->Init(parent, deadline);

        std::weak_ptr<TimerContext> weak_ctx = ctx;
        auto cancel_func = [weak_ctx]()
        {
            if (const auto s = weak_ctx.lock())
            {
                s->Cancel(ContextError::kCanceled);
            }
        };

        return {std::move(ctx), std::move(cancel_func)};
    }

    /** @brief 创建带相对超时的便捷工厂，等价于 WithDeadline(parent, now + timeout)。
     *  @note 模板实现位于 context_factory.h 头文件中。
     */
    // WithTimeout<Rep, Period> 为模板成员，实现在头文件中。

    /**
     * @brief 创建带键值对的上下文。
     *
     * 从 value_pool_ 获取 ValueContext，设置 parent + key + value。
     *
     * @param parent 父上下文
     * @param key 查找键（通常为静态全局变量地址）
     * @param value 关联值
     * @return ContextPtr
     */
    ContextPtr ContextFactory::WithValue(const ContextPtr& parent, const void* key, std::any value)
    {
        ValueContext* raw = value_pool_.Acquire();

        auto ctx = std::shared_ptr<ValueContext>(raw, [](ValueContext* p) { ContextFactory::ReleaseValue(p); });

        ctx->Set(parent, key, std::move(value));
        return ctx;
    }
} // namespace tyke::context
