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
    /** @brief 分发请求到对应路由的控制器和过滤器链。
     * @param request 待分发的请求对象。
     * @param response 用于填充响应的对象。
     */
    void DispatchRequest(const Request& request, Response& response);

    /** @brief 分发响应到等待中的异步回调或future。
     * @param response 待分发的响应对象。
     */
    void DispatchResponse(Response& response);
} // namespace tyke::dispatcher