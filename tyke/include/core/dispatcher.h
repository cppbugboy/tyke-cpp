/**
 * @file dispatcher.h
 * @brief 分发器声明。将请求/响应分发到对应路由的控制器和过滤器链。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include "component/context.h"
#include "core/request.h"

namespace tyke::dispatcher
{
void DispatchRequest(const Request &request, Response &response, const ContextPtr &context_ptr);

void DispatchResponse(const Response &response);
}// namespace tyke::dispatcher