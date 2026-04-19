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

#define RESPONSE_ROUTER_INSTANCE ResponseRouter::GetInstance()

    class ResponseRouter : public RouterBase<ResponseRouterGroup, ResponseRouter>
    {
        friend class Singleton<ResponseRouter>;
        friend class RouterBase<ResponseRouterGroup, ResponseRouter>;

    private:
        ResponseRouter() : RouterBase<ResponseRouterGroup, ResponseRouter>() {}
        ~ResponseRouter() override = default;
    };
}

