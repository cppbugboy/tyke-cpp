/**
 * @file response_controller.h
 * @brief 响应控制器基类
 * @author Nick
 * @date 2026/04/17
 *
 * ResponseController是响应控制器的抽象基类，用于注册响应路由处理器。
 */

#ifndef TYKE_RESPONSE_CONTROLLER_H
#define TYKE_RESPONSE_CONTROLLER_H

#include "controller_base.h"

namespace tyke
{
    /**
     * @brief 响应控制器抽象基类
     *
     * 继承自ControllerBase，专门用于处理响应路由。
     * 派生类需要实现RegisterMethod方法来注册响应处理器。
     */
    class ResponseController : public ControllerBase
    {
    public:
        ResponseController() = default;
        ~ResponseController() override = default;

        /**
         * @brief 注册响应路由方法
         *
         * 派生类实现此方法来注册响应处理器。
         */
        void RegisterMethod() override = 0;
    };
} // namespace tyke

#endif // TYKE_RESPONSE_CONTROLLER_H
