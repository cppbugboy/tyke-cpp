/**
 * @file object_pool.h
 * @brief 泛型对象池与 PooledPtr 智能指针。
 *
 * 提供线程安全的 ObjectPool<T> 以及自动回收的 PooledPtr<T>，
 * 避免裸指针导致的内存泄漏，简化对象复用。
 *
 * 使用注意：对象回池前务必调用其 Reset() 方法重置状态，
 * 通常在自定义删除器中完成。
 */

#pragma once

#include <mutex>
#include <vector>

namespace tyke
{
template<typename T>
class ObjectPool
{
public:
    ObjectPool() = default;

    explicit ObjectPool(size_t max_capacity) : max_capacity_(max_capacity)
    {
    }

    ~ObjectPool()
    { Clear(); }

    ObjectPool(const ObjectPool &)            = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;

    T *Acquire()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.empty())
        {
            return new T();
        }
        T *obj = pool_.back();
        pool_.pop_back();
        return obj;
    }

    void Release(T *obj)
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

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto *obj: pool_)
            delete obj;
        pool_.clear();
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<T *>   pool_;
    size_t             max_capacity_ = 0;
};

template<typename T>
class PooledPtr
{
public:
    static PooledPtr Acquire(ObjectPool<T> &pool)
    { return PooledPtr(pool.Acquire(), &pool); }

    PooledPtr() : ptr_(nullptr), pool_(nullptr)
    {
    }

    ~PooledPtr()
    { Reset(); }

    PooledPtr(PooledPtr &&other) noexcept : ptr_(other.ptr_), pool_(other.pool_)
    {
        other.ptr_  = nullptr;
        other.pool_ = nullptr;
    }

    PooledPtr &operator=(PooledPtr &&other) noexcept
    {
        if (this != &other)
        {
            Reset();
            ptr_        = other.ptr_;
            pool_       = other.pool_;
            other.ptr_  = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }

    PooledPtr(const PooledPtr &)            = delete;
    PooledPtr &operator=(const PooledPtr &) = delete;

    T *Get() const
    { return ptr_; }
    T &operator*() const
    { return *ptr_; }
    T *operator->() const
    { return ptr_; }
    explicit operator bool() const
    { return ptr_ != nullptr; }

    void Reset()
    {
        if (ptr_ && pool_)
        {
            pool_->Release(ptr_);
        }
        ptr_  = nullptr;
        pool_ = nullptr;
    }

private:
    PooledPtr(T *ptr, ObjectPool<T> *pool) : ptr_(ptr), pool_(pool)
    {
    }

    T             *ptr_;
    ObjectPool<T> *pool_;
};
}// namespace tyke