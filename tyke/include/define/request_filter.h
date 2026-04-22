/**
 * @file request_filter.h
 * @brief 请求过滤器接口声明。定义Before/After拦截方法，支持请求预处理和后处理。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include "core/tyke_request.h"

namespace tyke
{

    class RequestFilter
    {
    public:
        RequestFilter() = default;
        virtual ~RequestFilter() = default;


        virtual bool Before(const TykeRequest& request, TykeResponse& response) = 0;


        virtual bool After(const TykeRequest& request, TykeResponse& response) = 0;
    };
}

