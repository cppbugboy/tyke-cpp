/**
 * @file response_controller.h
 * @brief 响应控制器类型别名
 * @author Nick
 * @date 2026/04/17
 *
 * ResponseController是响应控制器的语义别名，用于注册响应路由处理器。
 * 实际类型为ControllerBase，两者完全等价。
 */

#pragma once

#include "controller_base.h"

namespace tyke
{
    using ResponseController = ControllerBase;
}