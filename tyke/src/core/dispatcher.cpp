/**
 * @file dispatcher.cpp
 * @brief 请求响应分发器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现Dispatcher类的具体逻辑，包括请求路由查找、过滤器链执行和处理器调用。
 */

#include "core/dispatcher.h"

#include "core/request_router.h"
#include "core/response_router.h"
#include "common/log_def.h"

namespace tyke
{
namespace dispatcher
{
/**
     * @brief 分发请求到相应的处理器
     * @param request 请求对象
     * @param response 响应对象
     *
     * 根据请求的路由信息查找对应的处理器和过滤器链，
     * 按顺序执行过滤器的Before方法，然后执行处理器，
     * 最后按逆序执行过滤器的After方法。
     */
    void DispatchRequest(const TykeRequest& request, TykeResponse& response)
    {
        LOG_DEBUG("Dispatching request: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());

        // 查找路由条目
        const auto route_entry = REQUEST_ROUTER_INSTANCE->GetRouteEntry(request.GetRoute());
        if (route_entry == nullptr)
        {
            LOG_WARN("Request route not found: {}", request.GetRoute());
            response.SetResult(kHttpStatusNotFound, "Not Found");
            return;
        }

        // 执行前置过滤器链
        for (auto it = route_entry->filter_chain.begin(); it != route_entry->filter_chain.end(); ++it)
        {
            LOG_DEBUG("Executing request filter Before: {}", typeid(**it).name());
            if (!(*it)->Before(request, response))
            {
                LOG_DEBUG("Request filter interrupted chain: {}", typeid(**it).name());
                return;
            }
        }

        // 执行处理器
        LOG_DEBUG("Executing request handler for route: {}", request.GetRoute());
        route_entry->handler(request, response);

        // 执行后置过滤器链（逆序）
        for (auto it = route_entry->filter_chain.rbegin(); it != route_entry->filter_chain.rend(); ++it)
        {
            LOG_DEBUG("Executing request filter After: {}", typeid(**it).name());
            if (!(*it)->After(request, response))
            {
                LOG_DEBUG("Request filter interrupted chain: {}", typeid(**it).name());
                return;
            }
        }

        LOG_DEBUG("Request dispatched successfully: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());
    }

    /**
     * @brief 分发响应到相应的处理器
     * @param response 响应对象
     *
     * 根据响应的路由信息查找对应的处理器和过滤器链，
     * 处理逻辑与请求分发类似。
     */
    void DispatchResponse(const TykeResponse& response)
    {
        LOG_DEBUG("Dispatching response: route={}, msg_uuid={}", response.GetRoute(), response.GetMsgUuid());

        // 查找路由条目
        const auto route_entry = RESPONSE_ROUTER_INSTANCE->GetRouteEntry(response.GetRoute());
        if (route_entry == nullptr)
        {
            LOG_WARN("Response route not found: {}", response.GetRoute());
            return;
        }

        // 执行前置过滤器链
        for (auto it = route_entry->filter_chain.begin(); it != route_entry->filter_chain.end(); ++it)
        {
            LOG_DEBUG("Executing response filter Before: {}", typeid(**it).name());
            if (!(*it)->Before(response))
            {
                LOG_DEBUG("Response filter interrupted chain: {}", typeid(**it).name());
                return;
            }
        }

        // 执行处理器
        LOG_DEBUG("Executing response handler for route: {}", response.GetRoute());
        route_entry->handler(response);

        // 执行后置过滤器链（逆序）
        for (auto it = route_entry->filter_chain.rbegin(); it != route_entry->filter_chain.rend(); ++it)
        {
            LOG_DEBUG("Executing response filter After: {}", typeid(**it).name());
            if (!(*it)->After(response))
            {
                LOG_DEBUG("Response filter interrupted chain: {}", typeid(**it).name());
                return;
            }
        }

        LOG_DEBUG("Response dispatched successfully: route={}, msg_uuid={}", response.GetRoute(), response.GetMsgUuid());
    }
}

} // tyke
