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
    void RegisterMethod();
    void HandleAsyncCallback(const tyke::Response& response);
    void HandleAsyncNotification(const tyke::Response& response);
}