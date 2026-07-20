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
    /** @brief 控制器自动注册辅助类。构造时立即调用注册函数，实现静态初始化时自动注册路由。 */
    class ControllerAutoRegister
    {
    public:
        ControllerAutoRegister() = default;

        /** @brief 构造时执行注册回调。 */
        explicit ControllerAutoRegister(void (*RegisterMethod)())
        {
            if (RegisterMethod)
            {
                RegisterMethod();
            }
        }
    };

    /** @brief 控制器基类。定义纯虚注册接口，子类在 RegisterMethod() 中注册请求/响应路由处理器。 */
    class ControllerBase
    {
    public:
        ControllerBase() = default;
        virtual ~ControllerBase() = default;

        /** @brief 纯虚方法：子类在此方法中注册路由和过滤器。 */
        virtual void RegisterMethod() = 0;
    };
} // namespace tyke
