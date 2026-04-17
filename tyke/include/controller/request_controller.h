/**
 * @file request_controller.h
 * @brief 请求控制器基类
 * @author Nick
 * @date 2026/04/17
 *
 * RequestController是请求控制器的抽象基类，用于注册请求路由处理器。
 */

#ifndef TYKE_REQUEST_CONTROLLER_H
#define TYKE_REQUEST_CONTROLLER_H

#include "controller_base.h"

namespace tyke
{
    /**
     * @brief 请求控制器抽象基类
     *
     * 继承自ControllerBase，专门用于处理请求路由。
     * 派生类需要实现RegisterMethod方法来注册请求处理器。
     */
    class RequestController : public ControllerBase
    {
    public:
        RequestController() = default;
        ~RequestController() override = default;

        /**
         * @brief 注册请求路由方法
         *
         * 派生类实现此方法来注册请求处理器。
         */
        void RegisterMethod() override = 0;
    };
} // namespace tyke

#endif // TYKE_REQUEST_CONTROLLER_H
