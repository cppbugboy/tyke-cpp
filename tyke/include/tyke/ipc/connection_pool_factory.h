/**
 * @file connection_pool_factory.h
 * @brief 连接池工厂声明。通过服务端UUID管理多个ConnectionPool实例的生命周期。
 * @author Nick
 * @date 2026/04/20
 *
 * @details
 * 单例工厂模式，提供按server_uuid索引的连接池管理功能：
 * - 按需创建连接池（惰性初始化）
 * - 移除指定服务端的连接池
 * - 全局关闭所有连接池
 *
 * @note 通过GetGlobalConnectionPoolFactory()获取全局单例。
 *       工厂本身是线程安全的。
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "connection_pool.h"

namespace tyke
{
    /**
     * @brief 连接池工厂类
     *
     * 管理多个ConnectionPool实例，以server_uuid为键进行索引。
     * 线程安全，支持多线程并发访问。
     */
    class ConnectionPoolFactory
    {
    public:
        ConnectionPoolFactory();
        ~ConnectionPoolFactory();

        /**
         * @brief 获取或创建指定服务端的连接池
         *
         * 如果server_uuid对应的连接池已存在则直接返回，
         * 否则创建新的连接池并缓存。
         *
         * @param server_uuid 服务端唯一标识符
         * @return std::shared_ptr<ConnectionPool> 连接池实例
         */
        std::shared_ptr<ConnectionPool> GetPool(const std::string& server_uuid);

        /**
         * @brief 移除并销毁指定服务端的连接池
         * @param server_uuid 要移除的服务端UUID
         */
        void RemovePool(const std::string& server_uuid);

        /**
         * @brief 关闭所有连接池
         *
         * 遍历并停止所有已创建的连接池，清空缓存。
         * 调用后工厂仍可继续使用，新的GetPool()将创建新池。
         */
        void Shutdown();

    private:
        std::mutex mutex_; ///< 保护pools_的互斥锁
        std::unordered_map<std::string, std::shared_ptr<ConnectionPool>> pools_; ///< server_uuid -> 连接池映射
    };

    /**
     * @brief 获取全局连接池工厂单例
     * @return ConnectionPoolFactory& 全局工厂实例引用
     */
    ConnectionPoolFactory& GetGlobalConnectionPoolFactory();
} // namespace tyke
