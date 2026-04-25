/**
 * @file response_router.h
 * @brief 响应路由器声明。继承RouterBase，管理响应路由表。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include "response_router_group.h"
#include "core/router_base.h"

namespace tyke
{
    class ResponseRouter : public RouterBase<ResponseRouterGroup>
    {
        friend class RouterBase<ResponseRouterGroup>;

    public:
        ResponseRouter() : RouterBase<ResponseRouterGroup>()
        {
        }

        ~ResponseRouter() = default;
    };

    inline ResponseRouter& GetGlobalResponseRouter()
    {
        static ResponseRouter instance;
        return instance;
    }
}

