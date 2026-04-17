/**
 * @file singleton.hpp
 * @brief 单例模式模板类
 * @author Nick
 * @date 2026/04/17
 *
 * Singleton是线程安全的单例模式模板类，使用std::call_once实现。
 * 继承此类即可获得单例特性。
 *
 * 使用示例：
 * @code
 *   class MyManager : public Singleton<MyManager> {
 *       friend class Singleton<MyManager>;
 *   private:
 *       MyManager() = default;
 *   };
 *   auto instance = MyManager::GetInstance();
 * @endcode
 */

#ifndef TYKE_SINGLETON_HPP
#define TYKE_SINGLETON_HPP

#include <memory>
#include <mutex>

namespace tyke
{
    /**
     * @brief 单例模式模板类
     * @tparam T 派生类类型
     *
     * 使用CRTP（Curiously Recurring Template Pattern）模式实现。
     * 线程安全，使用std::call_once保证只初始化一次。
     */
    template <typename T>
    class Singleton
    {
    public:
        /**
         * @brief 获取单例实例
         * @return 单例实例指针
         *
         * 线程安全的获取方法，首次调用时创建实例。
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

        // 禁止拷贝与移动
        Singleton(const Singleton&) = delete;
        Singleton& operator=(const Singleton&) = delete;
        Singleton(Singleton&&) = delete;
        Singleton& operator=(Singleton&&) = delete;

    protected:
        Singleton() = default;
        virtual ~Singleton() = default;

    private:
        static T* instance_;        ///< 单例实例指针
        static std::once_flag flag_; ///< 一次性初始化标志
    };

    // 静态成员定义
    template <typename T>
    T* Singleton<T>::instance_ = nullptr;

    template <typename T>
    std::once_flag Singleton<T>::flag_;
} // namespace tyke

#endif // TYKE_SINGLETON_HPP
