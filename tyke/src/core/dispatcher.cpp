#include "core/dispatcher.h"

#include <chrono>

#include "core/request_router.h"
#include "core/response_router.h"
#include "common/log_def.h"

namespace tyke::dispatcher
{
    void DispatchRequest(const TykeRequest& request, TykeResponse& response)
    {
        auto start = std::chrono::steady_clock::now();
        LOG_DEBUG("Dispatching request: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());

        const auto route_entry = REQUEST_ROUTER_INSTANCE->GetRouteEntry(request.GetRoute());
        if (route_entry == nullptr)
        {
            LOG_WARN("Request route not found: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());
            response.SetResult(kHttpStatusNotFound, "Not Found");
            return;
        }

        for (const auto& filter : route_entry->filter_chain)
        {
            LOG_DEBUG("Executing request filter Before: {}, msg_uuid={}", typeid(*filter).name(), request.GetMsgUuid());
            if (!filter->Before(request, response))
            {
                LOG_DEBUG("Request filter interrupted chain: {}, msg_uuid={}", typeid(*filter).name(), request.GetMsgUuid());
                return;
            }
        }

        LOG_DEBUG("Executing request handler for route: {}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());
        route_entry->handler(request, response);

        for (auto it = route_entry->filter_chain.rbegin(); it != route_entry->filter_chain.rend(); ++it)
        {
            LOG_DEBUG("Executing request filter After: {}, msg_uuid={}", typeid(**it).name(), request.GetMsgUuid());
            if (!(*it)->After(request, response))
            {
                LOG_DEBUG("Request filter interrupted chain: {}, msg_uuid={}", typeid(**it).name(), request.GetMsgUuid());
                return;
            }
        }

        auto end = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        LOG_INFO("Request dispatched: route={}, msg_uuid={}, elapsed={}ms", request.GetRoute(), request.GetMsgUuid(), elapsed_ms);
    }
    void DispatchResponse(const TykeResponse& response)
    {
        LOG_DEBUG("Dispatching response: route={}, msg_uuid={}", response.GetRoute(), response.GetMsgUuid());

        const auto route_entry = RESPONSE_ROUTER_INSTANCE->GetRouteEntry(response.GetRoute());
        if (route_entry == nullptr)
        {
            LOG_WARN("Response route not found: route={}, msg_uuid={}", response.GetRoute(), response.GetMsgUuid());
            return;
        }

        for (const auto& filter : route_entry->filter_chain)
        {
            LOG_DEBUG("Executing response filter Before: {}, msg_uuid={}", typeid(*filter).name(), response.GetMsgUuid());
            if (!filter->Before(response))
            {
                LOG_DEBUG("Response filter interrupted chain: {}, msg_uuid={}", typeid(*filter).name(), response.GetMsgUuid());
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
                LOG_DEBUG("Response filter interrupted chain: {}, msg_uuid={}", typeid(**it).name(), response.GetMsgUuid());
                return;
            }
        }

        LOG_DEBUG("Response dispatched successfully: route={}, msg_uuid={}", response.GetRoute(), response.GetMsgUuid());
    }
}
