/**
 * @file controller_base.h
 * @brief 控制器基类
 * @author Nick
 * @date 2026/04/17
 *
 * ControllerBase是所有控制器的抽象基类，定义了统一的注册接口。
 * 控制器用于处理特定路由的请求或响应。
 *
 * 使用示例：
 * @code
 *   class UserController : public RequestController {
 *   public:
 *       void RegisterMethod() override {
 *           REQUEST_ROUTER_INSTANCE->GetRoot()->AddRouteHandler("/user/login", &LoginHandler);
 *       }
 *   };
 *   TYKE_CONTROLLER_REGISTER(UserController)
 * @endcode
 */

#ifndef TYKE_CONTROLLER_BASE_H
#define TYKE_CONTROLLER_BASE_H

/**
 * @brief 控制器自动注册宏
 * @param ClassName 控制器类名
 *
 * 在全局作用域使用此宏，控制器将在程序启动时自动注册路由。
 */
#define TYKE_CONTROLLER_REGISTER(ClassName) \
static tyke::ControllerAutoRegister<ClassName> _auto_register_##ClassName;

namespace tyke
{
    /**
     * @brief 控制器抽象基类
     *
     * 所有控制器的基类，定义统一的注册接口。
     * 派生类需要实现RegisterMethod方法来注册路由处理器。
     */
    class ControllerBase
    {
    public:
        ControllerBase() = default;
        virtual ~ControllerBase() = default;

        /**
         * @brief 注册路由方法
         *
         * 派生类实现此方法来注册路由处理器。
         */
        virtual void RegisterMethod() = 0;
    };
}

#endif //TYKE_CONTROLLER_BASE_H
