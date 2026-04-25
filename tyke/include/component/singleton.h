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
         * @brief 获取单例实例的唯一入口
         * @return T& 返回子类实例的引用
         */
        static T& GetInstance()
        {
            // C++11 标准确保了静态局部变量在多线程环境下的初始化是线程安全的
            static T instance;
            return instance;
        }

        // 禁用拷贝构造函数
        Singleton(const Singleton&) = delete;
        // 禁用赋值操作符
        Singleton& operator=(const Singleton&) = delete;

        // 禁用移动构造和移动赋值（可选，但在单例中通常不需要）
        Singleton(Singleton&&) = delete;
        Singleton& operator=(Singleton&&) = delete;

    protected:
        // 构造函数设为 protected，允许子类调用，防止外部直接实例化
        Singleton() = default;

        // 析构函数设为 virtual 或 protected，取决于是否允许通过基类指针销毁
        virtual ~Singleton() = default;
    };
} // namespace tyke