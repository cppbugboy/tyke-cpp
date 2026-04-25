/**
 * @file context_factory.cpp
 */

#include "core/context_factory.h"

namespace tyke::context
{
    ContextPtr ContextFactory::Background()
    {
        static auto bg = std::make_shared<EmptyContext>();
        return bg;
    }

    ContextPtr ContextFactory::TODO()
    {
        return Background();
    }

    void ContextFactory::ReleaseCancel(CancelContext* ctx)
    {
        if (ctx)
        {
            ctx->Reset();
            cancel_pool_.Release(ctx);
        }
    }

    void ContextFactory::ReleaseTimer(TimerContext* ctx)
    {
        if (ctx)
        {
            ctx->Reset();
            timer_pool_.Release(ctx);
        }
    }

    void ContextFactory::ReleaseValue(ValueContext* ctx)
    {
        if (ctx)
        {
            ctx->Reset();
            value_pool_.Release(ctx);
        }
    }

    std::pair<ContextPtr, std::function<void()>> ContextFactory::WithCancel(const ContextPtr& parent)
    {
        CancelContext* raw = cancel_pool_.Acquire();

        // 使用自定义删除器：归还对象池
        auto ctx = std::shared_ptr<CancelContext>(raw, [](CancelContext* p)
        {
            ContextFactory::ReleaseCancel(p);
        });

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

    std::pair<ContextPtr, std::function<void()>> ContextFactory::WithDeadline(
        const ContextPtr& parent, const std::chrono::system_clock::time_point deadline)
    {
        TimerContext* raw = timer_pool_.Acquire();

        auto ctx = std::shared_ptr<TimerContext>(raw, [](TimerContext* p)
        {
            ContextFactory::ReleaseTimer(p);
        });

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

    ContextPtr ContextFactory::WithValue(const ContextPtr& parent, const void* key, std::any value)
    {
        ValueContext* raw = value_pool_.Acquire();

        auto ctx = std::shared_ptr<ValueContext>(raw, [](ValueContext* p)
        {
            ContextFactory::ReleaseValue(p);
        });

        ctx->Set(parent, key, std::move(value));
        return ctx;
    }
}
