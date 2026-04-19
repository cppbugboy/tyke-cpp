/**
 * @file controller_registry.h
 * @brief 控制器注册表声明。提供控制器与路由的自动注册机制。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef TYKE_CONTROLLER_REGISTRY_H
#define TYKE_CONTROLLER_REGISTRY_H

#include <memory>
#include <vector>
#include "controller_base.h"
#include "component/singleton.h"

namespace tyke
{

    class ControllerRegistry
    {
    public:
        ControllerRegistry() = delete;
        ~ControllerRegistry() = delete;
        ControllerRegistry(const ControllerRegistry&) = delete;
        ControllerRegistry& operator=(const ControllerRegistry&) = delete;
        ControllerRegistry(ControllerRegistry&&) = delete;
        ControllerRegistry& operator=(ControllerRegistry&&) = delete;


        static void RegisterController(ControllerBase* ctrl)
        {
            ctrl->RegisterMethod();
        }
    };


    template <typename T>
    class ControllerAutoRegister
    {
    public:

        ControllerAutoRegister()
        {
            ControllerRegistry::RegisterController(T::GetInstance());
        }
    };
}

#endif
