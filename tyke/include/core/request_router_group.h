/**
 * @file request_router_group.h
 * @brief 请求路由组类型定义
 * @author Nick
 * @date 2026/04/17
 *
 * 定义请求路由相关的类型别名，包括处理器函数类型和路由条目类型。
 */

#ifndef TYKE_REQUEST_ROUTER_GROUP_H
#define TYKE_REQUEST_ROUTER_GROUP_H
#include <functional>
#include <string>
#include <vector>

#include "filter/request_filter.h"
#include "router_group.h"
#include "tyke_request.h"

namespace tyke
{
    /// 请求处理器函数类型
    using RequestHandlerFunc = std::function<void(const TykeRequest&, TykeResponse&)>;

    /// 请求路由条目类型（包含处理器和过滤器链）
    using RequestRouteEntry = typename RouterGroup<RequestFilter, RequestHandlerFunc>::RouteEntry;

    /// 请求路由组类型
    using RequestRouterGroup = RouterGroup<RequestFilter, RequestHandlerFunc>;
} // tyke

#endif //TYKE_REQUEST_ROUTER_GROUP_H
