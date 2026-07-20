/**
 * @file example_response_controller.h
 * @brief 示例响应控制器声明。演示异步响应的路由注册和处理。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <string>

#include "tyke/tyke.h"

namespace controller::response::examples
{
    /**
     * @brief 向框架的 ResponseRouter 注册所有异步响应路由处理器。
     *
     * 在静态初始化期间通过 RESPONSE_CONTROLLER_REGISTER 宏自动调用。
     * 路由在 /api/async 子组下注册，用于 process、callback 和 notification 处理。
     */
    void RegisterMethod();

    /**
     * @brief 处理来自服务端的异步回调响应。
     *
     * 当匹配 /api/async/process 或 /api/async/callback 路由的
     * 异步响应到达时，由 ResponseRouter 调用。
     *
     * @param response 从服务端收到的异步响应。
     */
    void HandleAsyncCallback(const tyke::Response& response);

    /**
     * @brief 处理来自服务端的异步通知响应。
     *
     * 当匹配 /api/async/notification 路由的
     * 异步响应到达时，由 ResponseRouter 调用。
     *
     * @param response 从服务端收到的异步响应。
     */
    void HandleAsyncNotification(const tyke::Response& response);
}