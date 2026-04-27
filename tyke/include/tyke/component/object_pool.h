/**
 * @file object_pool.h
 * @brief 泛型对象池与 PooledPtr 智能指针。
 *
 * 本文件实现了一个线程安全的泛型对象池，用于高效的对象复用。
 * 对象池通过复用对象而非创建新对象来减少内存分配开销。
 *
 * @section features 主要特性
 * - 泛型实现，支持任意类型
 * - 使用互斥锁保证线程安全
 * - 可选的最大容量限制
 * - PooledPtr 智能指针，自动归还对象到池中
 *
 * @section usage 使用示例
 * @code
 * // 创建对象池
 * tyke::ObjectPool<MyStruct> pool;
 *
 * // 获取对象
 * MyStruct* obj = pool.Acquire();
 *
 * // 使用对象...
 *
 * // 归还对象
 * pool.Release(obj);
 *
 * // 或使用智能指针自动管理
 * auto ptr = tyke::PooledPtr<MyStruct>::Acquire(pool);
 * // ptr 离开作用域时自动归还
 * @endcode
 *
 * @author Nick
 * @date 2026/04/26
 */

#pragma once

#include <mutex>
#include <vector>

namespace tyke
{
    /**
     * @brief 泛型对象池模板类
     *
     * 提供线程安全的对象池实现，支持对象的获取、归还和清理。
     * 当池为空时，自动创建新对象；当池满时，丢弃归还的对象。
     *
     * @tparam T 池化对象的类型
     */
    template <typename T>
    class ObjectPool
    {
    public:
        /**
         * @brief 默认构造函数
         *
         * 创建一个无容量限制的对象池。
         */
        ObjectPool() = default;

        /**
         * @brief 带容量限制的构造函数
         *
         * @param max_capacity 池中保留的最大对象数量
         */
        explicit ObjectPool(size_t max_capacity) : max_capacity_(max_capacity)
        {
        }

        /**
         * @brief 析构函数
         *
         * 清理池中所有对象，释放内存。
         */
        ~ObjectPool()
        {
            Clear();
        }

        ObjectPool(const ObjectPool&) = delete;
        ObjectPool& operator=(const ObjectPool&) = delete;

        /**
         * @brief 从池中获取一个对象
         *
         * 如果池中包含对象，则取出一个返回；
         * 如果池为空，则使用 new 创建新对象。
         *
         * @return T* 对象指针，调用者负责后续管理
         *
         * @note 返回的对象可能包含之前使用的数据，
         *       建议在使用前调用对象的 Reset() 方法
         */
        [[nodiscard]] T* Acquire()
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
         *
         * 如果池已达到最大容量，则删除对象；
         * 否则将对象放入池中供后续复用。
         *
         * @param obj 要归还的对象指针
         *
         * @note 归还前建议调用对象的 Reset() 方法重置状态
         */
        void Release(T* obj)
        {
            if (!obj)
                return;
            std::lock_guard<std::mutex> lock(mutex_);
            if (max_capacity_ > 0 && pool_.size() >= max_capacity_)
            {
                delete obj;
                return;
            }
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
                delete obj;
            pool_.clear();
        }

        /**
         * @brief 获取池中当前对象数量
         *
         * @return size_t 池中对象数量
         */
        [[nodiscard]] size_t Size() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return pool_.size();
        }

    private:
        mutable std::mutex mutex_; ///< 保护池访问的互斥锁
        std::vector<T*> pool_; ///< 池化对象存储
        size_t max_capacity_ = 0; ///< 最大容量，0表示无限制
    };

    /**
     * @brief 池化对象智能指针
     *
     * RAII风格的智能指针，析构时自动将对象归还到池中。
     * 简化对象池的使用，避免忘记归还对象。
     *
     * @tparam T 指向的对象类型
     *
     * @section usage 使用示例
     * @code
     * ObjectPool<MyStruct> pool;
     * {
     *     auto ptr = PooledPtr<MyStruct>::Acquire(pool);
     *     ptr->DoSomething();
     *     // ptr 离开作用域，自动归还到池
     * }
     * @endcode
     */
    template <typename T>
    class PooledPtr
    {
    public:
        /**
         * @brief 从对象池获取对象并创建智能指针
         *
         * @param pool 对象池引用
         * @return PooledPtr 管理获取对象的智能指针
         */
        static PooledPtr Acquire(ObjectPool<T>& pool)
        {
            return PooledPtr(pool.Acquire(), &pool);
        }

        /**
         * @brief 默认构造函数
         *
         * 创建一个空指针。
         */
        PooledPtr() : ptr_(nullptr), pool_(nullptr)
        {
        }

        /**
         * @brief 析构函数
         *
         * 自动将对象归还到池中。
         */
        ~PooledPtr()
        {
            Reset();
        }

        /**
         * @brief 移动构造函数
         */
        PooledPtr(PooledPtr&& other) noexcept : ptr_(other.ptr_), pool_(other.pool_)
        {
            other.ptr_ = nullptr;
            other.pool_ = nullptr;
        }

        /**
         * @brief 移动赋值运算符
         */
        PooledPtr& operator=(PooledPtr&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                ptr_ = other.ptr_;
                pool_ = other.pool_;
                other.ptr_ = nullptr;
                other.pool_ = nullptr;
            }
            return *this;
        }

        PooledPtr(const PooledPtr&) = delete;
        PooledPtr& operator=(const PooledPtr&) = delete;

        /**
         * @brief 获取原始指针
         * @return T* 原始对象指针
         */
        T* Get() const
        {
            return ptr_;
        }

        /**
         * @brief 解引用运算符
         * @return T& 对象引用
         */
        T& operator*() const
        {
            return *ptr_;
        }

        /**
         * @brief 箭头运算符
         * @return T* 对象指针
         */
        T* operator->() const
        {
            return ptr_;
        }

        /**
         * @brief 布尔转换运算符
         * @return true 指针非空
         * @return false 指针为空
         */
        explicit operator bool() const
        {
            return ptr_ != nullptr;
        }

        /**
         * @brief 重置指针
         *
         * 将当前管理的对象归还到池中，并置空指针。
         */
        void Reset()
        {
            if (ptr_ && pool_)
            {
                pool_->Release(ptr_);
            }
            ptr_ = nullptr;
            pool_ = nullptr;
        }

    private:
        /**
         * @brief 私有构造函数
         *
         * @param ptr 对象指针
         * @param pool 对象池指针
         */
        PooledPtr(T* ptr, ObjectPool<T>* pool) : ptr_(ptr), pool_(pool)
        {
        }

        T* ptr_; ///< 管理的对象指针
        ObjectPool<T>* pool_; ///< 所属的对象池
    };
} // namespace tyke
