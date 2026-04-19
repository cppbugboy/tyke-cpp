/**
 * @file dispatcher.h
 * @brief 分发器声明。将请求/响应分发到对应路由的控制器和过滤器链。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef TYKE_DISPATCHER_H
#define TYKE_DISPATCHER_H
#include "core/tyke_request.h"

namespace tyke
{
    
namespace dispatcher
{

void DispatchRequest(const TykeRequest& request, TykeResponse& response);

void DispatchResponse(const TykeResponse& response);
}
}

#endif
