/**
 * @file request_router.h
 * @brief 请求路由器声明。继承RouterBase，管理请求路由表，支持分组路由和过滤器链。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include "tyke/core/router_base.h"
#include "request_router_group.h"

namespace tyke
{
    /** @brief 请求路由器。管理请求路由表，支持分组路由和过滤器链。 */
    class RequestRouter : public RouterBase<RequestRouterGroup>
    {
        friend class RouterBase<RequestRouterGroup>;

    public:
        RequestRouter() : RouterBase<RequestRouterGroup>()
        {
        }

        ~RequestRouter() = default;
    };

    /** @brief 获取全局请求路由器单例。 */
    inline RequestRouter& GetGlobalRequestRouter()
    {
        static RequestRouter instance;
        return instance;
    }
} // namespace tyke