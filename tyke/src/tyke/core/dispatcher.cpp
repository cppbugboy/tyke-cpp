/**
 * @file dispatcher.cpp
 * @brief 请求/响应分发器实现。按路由查找处理器并执行过滤器链（Before -> Handler -> After）。
 * @author Nick
 * @date 2026/04/20
 */

#include "tyke/core/dispatcher.h"

#include <chrono>

#include "tyke/common/log_def.h"
#include "tyke/core/request_router.h"
#include "tyke/core/request_stub.h"
#include "tyke/core/response_router.h"

namespace tyke::dispatcher
{
    /**
     * @brief 分发同步/异步请求到注册的处理器。
     *
     * 查找路由条目后按顺序执行：filter_chain.Before -> handler -> filter_chain.After（逆序）。
     * 过滤器的 Before 返回 false 会中断整个链。
     *
     * @param request 请求对象
     * @param response [out] 响应对象（由处理器或过滤器填充）
     */
    void DispatchRequest(const Request& request, Response& response)
    {
        LOG_DEBUG("Dispatching request: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());

        const auto route_entry = GetGlobalRequestRouter().GetRouteEntry(request.GetRoute());
        if (route_entry == nullptr)
        {
            LOG_WARN("Request route not found: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());
            response.SetResult(StatusCode::kRouteError, "Not Found");
            return;
        }

        for (const auto& filter : route_entry->filter_chain)
        {
            LOG_DEBUG("Executing request filter Before: {}, msg_uuid={}", typeid(*filter).name(), request.GetMsgUuid());
            if (!filter->Before(request, response))
            {
                LOG_DEBUG("Request filter interrupted chain: {}, msg_uuid={}", typeid(*filter).name(),
                          request.GetMsgUuid());
                return;
            }
        }

        LOG_DEBUG("Executing request handler for route: {}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());
        route_entry->handler(request, response);

        // After 过滤器逆序执行（洋葱模型）
        for (auto it = route_entry->filter_chain.rbegin(); it != route_entry->filter_chain.rend(); ++it)
        {
            LOG_DEBUG("Executing request filter After: {}, msg_uuid={}", typeid(**it).name(), request.GetMsgUuid());
            if (!(*it)->After(request, response))
            {
                LOG_DEBUG("Request filter interrupted chain: {}, msg_uuid={}", typeid(**it).name(),
                          request.GetMsgUuid());
                return;
            }
        }

        LOG_INFO("Request dispatched: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());
    }

    /**
     * @brief 分发异步响应到注册的处理器或 stub 回调。
     *
     * 优先查找路由条目执行过滤器链 + handler。
     * 若未找到路由条目，回退到 stub fallback：
     * - kResponseAsyncFunc -> stub::ExecFunc
     * - kResponseAsyncFuture -> stub::SetFuture
     *
     * @note 此 stub fallback 与 Go 版本行为一致：异步响应可绕过路由直接走回调/Future 机制。
     * @param response 响应对象
     */
    void DispatchResponse(Response& response)
    {
        LOG_DEBUG("Dispatching response: route={}, msg_uuid={}", response.GetRoute(), response.GetMsgUuid());

        const auto route_entry = GetGlobalResponseRouter().GetRouteEntry(response.GetRoute());
        if (route_entry == nullptr)
        {
            // stub fallback（与 Go 版本行为一致）
            if (response.GetMessageType() == MessageType::kResponseAsyncFunc)
            {
                stub::ExecFunc(response);
                return;
            }
            if (response.GetMessageType() == MessageType::kResponseAsyncFuture)
            {
                stub::SetFuture(std::move(response));
                return;
            }
            LOG_WARN("Response dropped: no route and no stub handler found: route={}, msg_uuid={}", response.GetRoute(),
                     response.GetMsgUuid());
            return;
        }

        for (const auto& filter : route_entry->filter_chain)
        {
            LOG_DEBUG("Executing response filter Before: {}, msg_uuid={}", typeid(*filter).name(),
                      response.GetMsgUuid());
            if (!filter->Before(response))
            {
                LOG_DEBUG("Response filter interrupted chain: {}, msg_uuid={}", typeid(*filter).name(),
                          response.GetMsgUuid());
                return;
            }
        }

        LOG_DEBUG("Executing response handler for route: {}, msg_uuid={}", response.GetRoute(), response.GetMsgUuid());
        route_entry->handler(response);

        for (auto it = route_entry->filter_chain.rbegin(); it != route_entry->filter_chain.rend(); ++it)
        {
            LOG_DEBUG("Executing response filter After: {}, msg_uuid={}", typeid(**it).name(), response.GetMsgUuid());
            if (!(*it)->After(response))
            {
                LOG_DEBUG("Response filter interrupted chain: {}, msg_uuid={}", typeid(**it).name(),
                          response.GetMsgUuid());
                return;
            }
        }

        LOG_DEBUG("Response dispatched successfully: route={}, msg_uuid={}", response.GetRoute(),
                  response.GetMsgUuid());
    }
} // namespace tyke::dispatcher
