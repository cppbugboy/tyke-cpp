/**
* @file context_factory.h
 * @brief Context 工厂类。封装对象池管理逻辑，提供统一的创建接口。
 *
 * 这是推荐的 Context 工厂 API。与 tyke::ContextPool（定义在 tyke/component/context.h）
 * 不同，ContextFactory 返回 std::pair<ContextPtr, std::function<void()>>，
 * 使调用者可以显式取消上下文。两者维护独立的对象池——未来版本将统一。
 */

#pragma once

#include <functional>
#include <memory>

#include "tyke/component/context.h"
#include "tyke/component/object_pool.h"

namespace tyke::context
{
    /** @brief Context 工厂类。提供统一的上下文创建接口，内部管理对象池。 */
    class ContextFactory
    {
    public:
        ContextFactory() = delete;

        /** @brief 获取根上下文（永不被取消的顶层上下文）。 */
        static ContextPtr Background();

        /** @brief 获取TODO上下文（占位用，不可用于实际传递）。 */
        static ContextPtr TODO();

        /** @brief 创建可取消的子上下文。
         * @param parent 父上下文。
         * @return pair<子上下文, 取消函数>。调用取消函数即取消该子上下文及所有派生上下文。
         */
        static std::pair<ContextPtr, std::function<void()>> WithCancel(const ContextPtr& parent);

        /** @brief 创建带截止时间的子上下文。
         * @param parent 父上下文。
         * @param deadline 绝对截止时间点，到期后自动取消。
         * @return pair<子上下文, 取消函数>。
         */
        static std::pair<ContextPtr, std::function<void()>> WithDeadline(const ContextPtr& parent,
                                                                         std::chrono::system_clock::time_point
                                                                         deadline);

        /** @brief 创建带超时时长的子上下文（相对于当前时间）。
         * @tparam Rep duration 的数值类型。
         * @tparam Period duration 的时间单位。
         * @param parent 父上下文。
         * @param timeout 超时时长。
         * @return pair<子上下文, 取消函数>。
         */
        template <typename Rep, typename Period>
        static std::pair<ContextPtr, std::function<void()>> WithTimeout(const ContextPtr& parent,
                                                                        std::chrono::duration<Rep, Period> timeout)
        {
            return WithDeadline(parent, std::chrono::system_clock::now() + timeout);
        }

        /** @brief 创建带键值对的子上下文。
         * @param parent 父上下文。
         * @param key 值的唯一键（指针地址）。
         * @param value 任意类型的值。
         * @return 子上下文 shared_ptr。
         */
        static ContextPtr WithValue(const ContextPtr& parent, const void* key, std::any value);

    private:
        /** @brief 回收 CancelContext 到对象池。 */
        static void ReleaseCancel(CancelContext* ctx);
        /** @brief 回收 TimerContext 到对象池。 */
        static void ReleaseTimer(TimerContext* ctx);
        /** @brief 回收 ValueContext 到对象池。 */
        static void ReleaseValue(ValueContext* ctx);

        /** @brief CancelContext 对象池（容量1024）。 */
        inline static ObjectPool<CancelContext> cancel_pool_{1024};
        /** @brief TimerContext 对象池（容量1024）。 */
        inline static ObjectPool<TimerContext> timer_pool_{1024};
        /** @brief ValueContext 对象池（容量1024）。 */
        inline static ObjectPool<ValueContext> value_pool_{1024};
    };
} // namespace tyke::context
