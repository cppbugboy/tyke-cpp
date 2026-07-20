/**
 * @file response_filter.h
 * @brief 响应过滤器接口声明。定义Before/After拦截方法，支持响应预处理和后处理。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include "tyke/core/response.h"

namespace tyke
{
    /** @brief 响应过滤器抽象接口。在响应处理前/后执行拦截逻辑。 */
    class ResponseFilter
    {
    public:
        ResponseFilter() = default;
        virtual ~ResponseFilter() = default;

        /** @brief 响应处理前调用。返回 false 可中断后续过滤器链和处理器。
         * @param response 响应对象。
         * @return true 继续处理链，false 中断处理。
         */
        virtual bool Before(const Response& response) = 0;

        /** @brief 响应处理后调用。可用于日志记录或后处理。
         * @param response 响应对象。
         * @return true 继续处理链，false 中断处理。
         */
        virtual bool After(const Response& response) = 0;
    };
} // namespace tyke