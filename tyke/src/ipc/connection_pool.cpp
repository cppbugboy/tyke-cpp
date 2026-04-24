/**
 * @file connection_pool.cpp
 * @brief 连接池实现。管理IpcConnection的获取、归还、健康检查和空闲清理。
 * @author Nick
 * @date 2026/04/20
 */

#include "ipc/connection_pool.h"
#include "common/log_def.h"

namespace tyke
{
    ConnectionPool::ConnectionPool(const std::string_view server_uuid)
        : server_uuid_(server_uuid)
    {
        LOG_INFO("Connection pool created, server={}: ", server_uuid_);
    }

    ConnectionPool::~ConnectionPool() = default;

    TResult<IpcConnection*> ConnectionPool::Acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        while (!connections_vec_.empty())
        {
            IpcConnection* conn = connections_vec_.back();
            connections_vec_.pop_back();

            if (conn->IsValid())
            {
                return conn;
            }
            else
            {
                LOG_WARN("Idle connection invalid, destroying, server={}", server_uuid_);
                delete conn;
            }
        }

        if (IpcConnection* conn = CreateConnection())
        {
            LOG_DEBUG("Created new connection in pool, server={}", server_uuid_);
            return conn;
        }
        return nonstd::make_unexpected("failed to create connection for pool");
    }

    void ConnectionPool::Release(IpcConnection* conn, bool should_reconnect)
    {
        if (!conn)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        if (should_reconnect || !conn->IsValid())
        {
            LOG_WARN("Releasing broken connection, reconnecting, server={}", server_uuid_);
            delete conn;
        }
        else
        {
            connections_vec_.push_back(conn);
            LOG_DEBUG("Released connection to pool, server={}", server_uuid_);
        }

        acquire_cv_.notify_one();
    }

    const std::string& ConnectionPool::GetServerUuid() const
    {
        return server_uuid_;
    }

    void ConnectionPool::Stop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& conn : connections_vec_)
        {
            conn->Close();
        }
        connections_vec_.clear();
    }

    IpcConnection* ConnectionPool::CreateConnection()
    {
        const auto conn = new IpcConnection();
        if (auto result = conn->Connect(server_uuid_, kIpcDefaultTimeoutMs); !result)
        {
            LOG_ERROR("Failed to connect new connection, server={}, error={}", server_uuid_, result.error());
            delete conn;
            return nullptr;
        }
        LOG_DEBUG("Created and connected new connection, server={}", server_uuid_);
        return conn;
    }
}