/**
* @file context_factory.h
 * @brief Context 工厂类。封装对象池管理逻辑，提供统一的创建接口。
 */

#pragma once

#include <functional>
#include <memory>

#include "component/context.h"
#include "component/object_pool.h"

namespace tyke::context
{
    class ContextFactory
    {
    public:
        ContextFactory() = delete;

        // 获取根上下文
        static ContextPtr Background();
        static ContextPtr TODO();

        // 从池中获取子上下文
        static std::pair<ContextPtr, std::function<void()>> WithCancel(const ContextPtr& parent);

        static std::pair<ContextPtr, std::function<void()>> WithDeadline(const ContextPtr& parent,
                                                                         std::chrono::system_clock::time_point
                                                                         deadline);

        template <typename Rep, typename Period>
        static std::pair<ContextPtr, std::function<void()>> WithTimeout(const ContextPtr& parent,
                                                                        std::chrono::duration<Rep, Period> timeout)
        {
            return WithDeadline(parent, std::chrono::system_clock::now() + timeout);
        }

        static ContextPtr WithValue(const ContextPtr& parent, const void* key, std::any value);

    private:
        // 内部回收接口
        static void ReleaseCancel(CancelContext* ctx);
        static void ReleaseTimer(TimerContext* ctx);
        static void ReleaseValue(ValueContext* ctx);

        // 静态对象池
        inline static ObjectPool<CancelContext> cancel_pool_{1024};
        inline static ObjectPool<TimerContext> timer_pool_{1024};
        inline static ObjectPool<ValueContext> value_pool_{1024};
    };
} // namespace tyke::context
