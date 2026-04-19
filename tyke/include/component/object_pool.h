/**
 * @file object_pool.h
 * @brief 对象池模板类
 * @author Nick
 * @date 2026/04/19
 *
 * 线程安全的对象复用池，减少频繁内存分配开销。
 * 通过Acquire获取对象、Release归还对象实现对象复用。
 */

#pragma once

#include <mutex>
#include <vector>

namespace tyke
{
    /**
     * @brief 对象池模板类
     *
     * 线程安全的对象复用池。Acquire时优先从池中取空闲对象，
     * 池为空时创建新对象；Release时将对象归还池中。
     * 对象在池销毁时统一释放。
     *
     * @tparam T 池化管理的对象类型
     */
    template <typename T>
    class ObjectPool
    {
    public:
        ObjectPool() = default;

        ~ObjectPool()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto* obj : pool_)
            {
                delete obj;
            }
            pool_.clear();
        }

        ObjectPool(const ObjectPool&) = delete;
        ObjectPool& operator=(const ObjectPool&) = delete;

        /**
         * @brief 从池中获取一个对象
         * @return T* 对象指针，池为空时创建新对象
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
         * @brief 将对象归还到池中
         * @param obj 待归还的对象指针，为nullptr时忽略
         */
        void Release(T* obj)
        {
            if (!obj)
                return;
            std::lock_guard<std::mutex> lock(mutex_);
            pool_.push_back(obj);
        }

        /**
         * @brief 清空池中所有对象并释放内存
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
        std::mutex mutex_;
        std::vector<T*> pool_;
    };
}
