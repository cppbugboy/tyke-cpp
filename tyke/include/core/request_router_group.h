/**
 * @file request_router_group.h
 * @brief 请求路由分组声明。支持层级路由结构，每个分组可包含子分组和路由条目。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "filter/request_filter.h"
#include "router_group.h"
#include "tyke_request.h"

namespace tyke
{

    using RequestHandlerFunc = std::function<void(const TykeRequest&, TykeResponse&)>;

    using RequestRouteEntry = RouterGroup<RequestFilter, RequestHandlerFunc>::RouteEntry;

    using RequestRouterGroup = RouterGroup<RequestFilter, RequestHandlerFunc>;
}
