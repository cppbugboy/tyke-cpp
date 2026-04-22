/**
 * @file controller_base.h
 * @brief 控制器基类声明。定义请求/响应处理器的纯虚接口。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#define TYKE_CONTROLLER_REGISTER(ClassName) \
static tyke::ControllerAutoRegister<ClassName> _auto_register_##ClassName;


namespace tyke
{
    template <typename T>
    class ControllerAutoRegister
    {
    public:
        ControllerAutoRegister()
        {
            T::GetInstance().RegisterMethod();
        }
    };


    class ControllerBase
    {
    public:
        ControllerBase() = default;
        virtual ~ControllerBase() = default;


        virtual void RegisterMethod() = 0;
    };
}
