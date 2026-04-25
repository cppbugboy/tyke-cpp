/**
 * @file controller_base.h
 * @brief 控制器基类声明。定义请求/响应处理器的纯虚接口。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#define REQUEST_CONTROLLER_REGISTER(ControllerFlag, RegisterMethod)                                                    \
    static tyke::ControllerAutoRegister _auto_register_request_##ControllerFlag(RegisterMethod);

#define RESPONSE_CONTROLLER_REGISTER(ControllerFlag, RegisterMethod)                                                   \
    static tyke::ControllerAutoRegister _auto_register_response_##ControllerFlag(RegisterMethod);

namespace tyke
{
    class ControllerAutoRegister
    {
    public:
        ControllerAutoRegister() = default;

        explicit ControllerAutoRegister(void (*RegisterMethod)())
        {
            if (RegisterMethod)
            {
                RegisterMethod();
            }
        }
    };


    class ControllerBase
    {
    public:
        ControllerBase() = default;
        virtual ~ControllerBase() = default;


        virtual void RegisterMethod() = 0;
    };
} // namespace tyke
