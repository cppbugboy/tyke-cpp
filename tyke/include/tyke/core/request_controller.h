/**
 * @file request_controller.h
 * @brief 请求控制器类型别名
 * @author Nick
 * @date 2026/04/17
 *
 * RequestController是请求控制器的语义别名，用于注册请求路由处理器。
 * 实际类型为ControllerBase，两者完全等价。
 */

#pragma once

#include "controller_base.h"

namespace tyke
{
    /** @brief 请求控制器类型别名。等价于 ControllerBase，语义上用于请求路由注册。 */
    using RequestController = ControllerBase;
}