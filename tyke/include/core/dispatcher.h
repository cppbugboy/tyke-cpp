/**
 * @file dispatcher.h
 * @brief 请求响应分发器
 * @author Nick
 * @date 2026/04/17
 *
 * Dispatcher实现请求和响应的路由分发逻辑，根据请求路由查找对应的处理器和过滤器链，
 * 并按顺序执行过滤器和处理器。
 */

#ifndef TYKE_DISPATCHER_H
#define TYKE_DISPATCHER_H
#include "core/tyke_request.h"

namespace tyke
{
    /**
     * @brief 请求响应分发器类
     *
     * 提供静态方法用于分发请求和响应到相应的处理器，
     * 负责管理过滤器链的执行顺序。
     *
     * 分发流程：
     * 1. 根据路由查找路由条目
     * 2. 按顺序执行过滤器的Before方法
     * 3. 执行处理器
     * 4. 按逆序执行过滤器的After方法
     */
namespace dispatcher
{
/**
 * @brief 分发请求到相应的处理器
 * @param request 请求对象
 * @param response 响应对象
 *
 * 根据请求的路由信息查找对应的处理器和过滤器链，
 * 按顺序执行过滤器的Before方法，然后执行处理器，
 * 最后按逆序执行过滤器的After方法。
 */
void DispatchRequest(const TykeRequest& request, TykeResponse& response);

/**
 * @brief 分发响应到相应的处理器
 * @param response 响应对象
 *
 * 根据响应的路由信息查找对应的处理器和过滤器链，
 * 处理逻辑与请求分发类似。
 */
void DispatchResponse(const TykeResponse& response);
}
} // tyke

#endif //TYKE_DISPATCHER_H
