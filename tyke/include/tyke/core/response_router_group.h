/**
 * @file response_router_group.h
 * @brief 响应路由分组声明。支持层级路由结构，每个分组可包含子分组和路由条目。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "response.h"
#include "response_filter.h"
#include "router_group.h"

namespace tyke
{
    /** @brief 响应处理函数类型：接收响应的回调。 */
    using ResponseHandlerFunc = std::function<void(const Response &)>;

    /** @brief 响应路由条目：包含处理函数和过滤器链。 */
    using ResponseRouteEntry = typename RouterGroup<ResponseFilter, ResponseHandlerFunc>::RouteEntry;

    /** @brief 响应路由分组类型。 */
    using ResponseRouterGroup = RouterGroup<ResponseFilter, ResponseHandlerFunc>;
} // namespace tyke