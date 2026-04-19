/**
 * @file singleton.h
 * @brief 单例模式模板类 (C++17)
 * @author Nick
 * @date 2026/04/19
 *
 * 使用CRTP模式和std::call_once实现线程安全单例。
 * 派生类需将Singleton<Derived>声明为友元。
 *
 * C++17特性:
 * - 使用inline static成员变量，消除模板类外部定义的需要
 */

#pragma once

#include <mutex>

namespace tyke
{
    /**
     * @brief 单例模式模板基类
     *
     * 使用CRTP模式，派生类通过Singleton<Derived>继承即可获得单例语义。
     * 线程安全，使用std::call_once保证只初始化一次。
     * 派生类需将Singleton<Derived>声明为友元以访问受保护构造函数。
     *
     * @tparam T 派生类类型
     */
    template <typename T>
    class Singleton
    {
    public:
        /**
         * @brief 获取单例实例指针
         * @return T* 单例实例指针，首次调用时创建实例
         */
        static T* GetInstance()
        {
            std::call_once(flag_, []()
            {
                static T instance;
                instance_ = &instance;
            });
            return instance_;
        }

        Singleton(const Singleton&) = delete;
        Singleton& operator=(const Singleton&) = delete;
        Singleton(Singleton&&) = delete;
        Singleton& operator=(Singleton&&) = delete;

    protected:
        Singleton() = default;
        virtual ~Singleton() = default;

    private:
        inline static T* instance_ = nullptr;
        inline static std::once_flag flag_;
    };
}
