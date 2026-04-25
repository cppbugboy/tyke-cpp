/**
 * @file connection_pool_factory.cpp
 * @brief 连接池工厂实现。通过服务端UUID管理多个连接池实例。
 * @author Nick
 * @date 2026/04/20
 */

#include "ipc/connection_pool_factory.h"

#include "common/log_def.h"

namespace tyke
{
    ConnectionPoolFactory::ConnectionPoolFactory() = default;

    ConnectionPoolFactory::~ConnectionPoolFactory()
    {
        Shutdown();
    }

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

    ConnectionPoolFactory& GetGlobalConnectionPoolFactory()
    {
        static ConnectionPoolFactory instance;
        return instance;
    }
} // namespace tyke