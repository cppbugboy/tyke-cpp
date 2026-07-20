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

#include "request.h"
#include "request_filter.h"
#include "router_group.h"

namespace tyke
{
    /** @brief 请求处理函数类型：接收请求和响应引用的回调。 */
    using RequestHandlerFunc = std::function<void(const Request &, Response &)>;

    /** @brief 请求路由条目：包含处理函数和过滤器链。 */
    using RequestRouteEntry = RouterGroup<RequestFilter, RequestHandlerFunc>::RouteEntry;

    /** @brief 请求路由分组类型。 */
    using RequestRouterGroup = RouterGroup<RequestFilter, RequestHandlerFunc>;
} // namespace tyke