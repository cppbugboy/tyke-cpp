/**
 * @file response_router_group.h
 * @brief 响应路由组类型定义
 * @author Nick
 * @date 2026/04/17
 *
 * 定义响应路由相关的类型别名，包括处理器函数类型和路由条目类型。
 */

#ifndef TYKE_RESPONSE_ROUTER_GROUP_H
#define TYKE_RESPONSE_ROUTER_GROUP_H
#include <functional>
#include <string>
#include <vector>

#include "filter/response_filter.h"
#include "router_group.h"
#include "tyke_response.h"

namespace tyke
{
    /// 响应处理器函数类型
    using ResponseHandlerFunc = std::function<void(const TykeResponse&)>;

    /// 响应路由条目类型（包含处理器和过滤器链）
    using ResponseRouteEntry = typename RouterGroup<ResponseFilter, ResponseHandlerFunc>::RouteEntry;

    /// 响应路由组类型
    using ResponseRouterGroup = RouterGroup<ResponseFilter, ResponseHandlerFunc>;
} // tyke

#endif //TYKE_RESPONSE_ROUTER_GROUP_H
