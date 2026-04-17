/**
 * @file controller_registry.h
 * @brief 控制器注册表
 * @author Nick
 * @date 2026/04/17
 *
 * ControllerRegistry提供控制器注册功能，配合TYKE_CONTROLLER_REGISTER宏实现自动注册。
 */

#ifndef TYKE_CONTROLLER_REGISTRY_H
#define TYKE_CONTROLLER_REGISTRY_H

#include <memory>
#include <vector>
#include "controller_base.h"
#include "component/singleton.hpp"

namespace tyke
{
    /**
     * @brief 控制器注册表类
     *
     * 提供静态方法用于注册控制器。
     * 所有方法都是静态的，无需实例化。
     */
    class ControllerRegistry
    {
    public:
        ControllerRegistry() = delete;
        ~ControllerRegistry() = delete;
        ControllerRegistry(const ControllerRegistry&) = delete;
        ControllerRegistry& operator=(const ControllerRegistry&) = delete;
        ControllerRegistry(ControllerRegistry&&) = delete;
        ControllerRegistry& operator=(ControllerRegistry&&) = delete;

        /**
         * @brief 注册控制器
         * @param ctrl 控制器指针
         *
         * 调用控制器的RegisterMethod方法进行路由注册。
         */
        static void RegisterController(ControllerBase* ctrl)
        {
            ctrl->RegisterMethod();
        }
    };

    /**
     * @brief 控制器自动注册辅助模板类
     * @tparam T 控制器类型
     *
     * 在全局作用域声明此类的静态实例，控制器将在程序启动时自动注册。
     */
    template <typename T>
    class ControllerAutoRegister
    {
    public:
        /**
         * @brief 构造函数
         *
         * 构造时自动向全局注册表添加派生类实例。
         */
        ControllerAutoRegister()
        {
            ControllerRegistry::RegisterController(T::GetInstance());
        }
    };
} // namespace tyke

#endif // TYKE_CONTROLLER_REGISTRY_H
