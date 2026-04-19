/**
 * @file controller_registry.h
 * @brief 控制器注册表声明。提供控制器与路由的自动注册机制。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

namespace tyke
{
    template <typename T>
    class ControllerAutoRegister
    {
    public:

        ControllerAutoRegister()
        {
            T::GetInstance()->RegisterMethod();
        }
    };
}
