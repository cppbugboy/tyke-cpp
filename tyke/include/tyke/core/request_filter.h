/**
 * @file request_filter.h
 * @brief 请求过滤器接口声明。定义Before/After拦截方法，支持请求预处理和后处理。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include "tyke/component/context.h"
#include "tyke/core/request.h"

namespace tyke
{
    /** @brief 请求过滤器抽象接口。在请求处理前/后执行拦截逻辑。 */
    class RequestFilter
    {
    public:
        RequestFilter() = default;
        virtual ~RequestFilter() = default;

        /** @brief 请求处理前调用。返回 false 可中断后续过滤器链和处理器。
         * @param request 请求对象。
         * @param response 可预填充的响应对象（如拒绝访问）。
         * @return true 继续处理链，false 中断处理。
         */
        virtual bool Before(const Request& request, Response& response) = 0;

        /** @brief 请求处理后调用。可对响应进行修改或记录。
         * @param request 原始请求对象。
         * @param response 处理器产生的响应对象。
         * @return true 继续处理链，false 中断处理。
         */
        virtual bool After(const Request& request, Response& response) = 0;
    };
} // namespace tyke
