/**
 * @file response_filter.h
 * @brief 响应过滤器接口声明。定义Before/After拦截方法，支持响应预处理和后处理。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include "core/tyke_response.h"

namespace tyke
{

    class ResponseFilter
    {
    public:
        ResponseFilter() = default;
        virtual ~ResponseFilter() = default;


        virtual bool Before(const TykeResponse& response) = 0;


        virtual bool After(const TykeResponse& response) = 0;
    };
}
