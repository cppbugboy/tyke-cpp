/**
 * @file object_pool.hpp
 * @brief 对象池模板类
 * @author Nick
 * @date 2026/04/17
 *
 * ObjectPool是线程安全的对象池模板类，用于对象的复用，减少内存分配开销。
 * 适用于频繁创建和销毁的对象，如请求/响应对象。
 *
 * 使用示例：
 * @code
 *   ObjectPool<MyObject> pool;
 *   MyObject* obj = pool.Acquire();
 *   // 使用obj...
 *   pool.Release(obj);
 * @endcode
 */

#ifndef TYKE_OBJECT_POOL_H
#define TYKE_OBJECT_POOL_H

#include <mutex>
#include <vector>

namespace tyke
{
    /**
     * @brief 对象池模板类
     * @tparam T 对象类型
     *
     * 线程安全的对象池，支持对象的获取和释放。
     * 池为空时自动创建新对象，释放时将对象归还池中。
     */
    template <typename T>
    class ObjectPool
    {
    public:
        /**
         * @brief 构造函数
         */
        ObjectPool() = default;

        /**
         * @brief 析构函数
         *
         * 释放池中所有对象。
         */
        ~ObjectPool()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto* obj : pool_)
            {
                delete obj;
            }
            pool_.clear();
        }

        // 禁用拷贝和赋值，防止资源被意外接管或释放
        ObjectPool(const ObjectPool&) = delete;
        ObjectPool& operator=(const ObjectPool&) = delete;

        /**
         * @brief 从池中获取对象
         * @return 对象指针
         *
         * 池为空时创建新对象，否则返回池中的对象。
         */
        T* Acquire()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pool_.empty())
            {
                return new T();
            }
            T* obj = pool_.back();
            pool_.pop_back();
            return obj;
        }

        /**
         * @brief 将对象释放回池中
         * @param obj 对象指针
         *
         * 将对象归还池中以供复用。
         */
        void Release(T* obj)
        {
            if (!obj)
                return;
            std::lock_guard<std::mutex> lock(mutex_);
            pool_.push_back(obj);
        }

        /**
         * @brief 清空池中所有对象
         *
         * 删除池中所有对象并清空池。
         */
        void Clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto* obj : pool_)
            {
                delete obj;
            }
            pool_.clear();
        }

    private:
        std::mutex mutex_;      ///< 互斥锁，保证线程安全
        std::vector<T*> pool_;  ///< 对象池
    };
} // tyke

#endif //TYKE_OBJECT_POOL_H
