/**
 * @file dispatcher.h
 * @brief 分发器声明。将请求/响应分发到对应路由的控制器和过滤器链。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include "tyke/core/request.h"

namespace tyke::dispatcher
{
    void DispatchRequest(const Request& request, Response& response);

    void DispatchResponse(const Response& response);
} // namespace tyke::dispatcher