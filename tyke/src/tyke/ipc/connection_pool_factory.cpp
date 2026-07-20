/**
 * @file connection_pool_factory.cpp
 * @brief 连接池工厂实现。按服务端 UUID 管理多个 ConnectionPool 实例，提供获取、移除和全局关闭功能。
 * @author Nick
 * @date 2026/04/20
 */

#include "tyke/ipc/connection_pool_factory.h"

#include "tyke/common/log_def.h"

namespace tyke
{
    ConnectionPoolFactory::ConnectionPoolFactory() = default;

    ConnectionPoolFactory::~ConnectionPoolFactory()
    {
        Shutdown();
    }

    /**
     * @brief 获取或创建指定服务端的连接池。
     *
     * 线程安全：若池不存在则创建，否则返回已有实例。
     *
     * @param server_uuid 服务端 UUID
     * @return shared_ptr<ConnectionPool> 连接池实例
     */
    std::shared_ptr<ConnectionPool> ConnectionPoolFactory::GetPool(const std::string& server_uuid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (const auto it = pools_.find(server_uuid); it != pools_.end())
        {
            return it->second;
        }

        const auto pool = std::make_shared<ConnectionPool>(server_uuid);
        pools_[server_uuid] = pool;
        LOG_INFO("Created new connection pool, server={}", server_uuid);
        return pool;
    }

    /** @brief 移除并停止指定服务端的连接池。 */
    void ConnectionPoolFactory::RemovePool(const std::string& server_uuid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (const auto it = pools_.find(server_uuid); it != pools_.end())
        {
            it->second->Stop();
            pools_.erase(it);
            LOG_INFO("Removed connection pool, server={}", server_uuid);
        }
    }

    /** @brief 关闭所有连接池并清空映射。 */
    void ConnectionPoolFactory::Shutdown()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [uuid, pool] : pools_)
        {
            pool->Stop();
            LOG_INFO("Stopped connection pool, server={}", uuid);
        }
        pools_.clear();
    }

    /** @brief 获取全局连接池工厂单例。 */
    ConnectionPoolFactory& GetGlobalConnectionPoolFactory()
    {
        static ConnectionPoolFactory instance;
        return instance;
    }
} // namespace tyke
