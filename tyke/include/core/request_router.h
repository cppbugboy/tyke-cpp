/**
 * @file request_router.h
 * @brief 请求路由器声明。继承RouterBase，管理请求路由表，支持分组路由和过滤器链。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include "core/router_base.h"
#include "request_router_group.h"

namespace tyke
{
    class RequestRouter : public RouterBase<RequestRouterGroup>
    {
        friend class RouterBase<RequestRouterGroup>;

    public:
        RequestRouter() : RouterBase<RequestRouterGroup>()
        {
        }

        ~RequestRouter() = default;
    };

    inline RequestRouter& GetGlobalRequestRouter()
    {
        static RequestRouter instance;
        return instance;
    }
} // namespace tyke