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

#include "define/response_filter.h"
#include "response.h"
#include "router_group.h"

namespace tyke
{
using ResponseHandlerFunc = std::function<void(const Response &)>;

using ResponseRouteEntry = typename RouterGroup<ResponseFilter, ResponseHandlerFunc>::RouteEntry;

using ResponseRouterGroup = RouterGroup<ResponseFilter, ResponseHandlerFunc>;
}// namespace tyke