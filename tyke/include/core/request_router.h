/**
 * @file request_router.h
 * @brief 请求路由器声明。继承RouterBase，管理请求路由表，支持分组路由和过滤器链。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef TYKE_ROUTER_H
#define TYKE_ROUTER_H
#include "request_router_group.h"
#include "core/router_base.h"

namespace tyke
{

#define REQUEST_ROUTER_INSTANCE RequestRouter::GetInstance()

    class RequestRouter : public RouterBase<RequestRouterGroup, RequestRouter>
    {
        friend class Singleton<RequestRouter>;
        friend class RouterBase<RequestRouterGroup, RequestRouter>;

    private:
        RequestRouter() : RouterBase<RequestRouterGroup, RequestRouter>() {}
        ~RequestRouter() override = default;
    };
}

#endif
