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
ConnectionPool::ConnectionPool(const std::string_view server_uuid, const ConnectionPoolConfig &config)
    : server_uuid_(server_uuid), config_(config)
{ LOG_INFO("Connection pool created, server={}", server_uuid_); }

ConnectionPool::~ConnectionPool() = default;

TResult<IpcConnection *> ConnectionPool::Acquire()
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (!connections_vec_.empty())
    {
        auto conn_uptr = std::move(connections_vec_.back());
        connections_vec_.pop_back();
        lock.unlock();

        if (conn_uptr->IsValid())
        {
            IpcConnection *raw = conn_uptr.release();
            return raw;
        }

        LOG_WARN("Idle connection invalid, destroying, server={}", server_uuid_);
        total_connections_.fetch_sub(1, std::memory_order_relaxed);
        lock.lock();
    }

    if (config_.max_connections > 0 && total_connections_.load(std::memory_order_relaxed) >= config_.max_connections)
    {
        if (!acquire_cv_.wait_for(lock, std::chrono::milliseconds(config_.acquire_timeout_ms),
                                  [this] { return !connections_vec_.empty() ||
                                                  total_connections_.load(std::memory_order_relaxed) < config_.max_connections; }))
        {
            return nonstd::make_unexpected("connection pool exhausted, max=" + std::to_string(config_.max_connections));
        }

        if (!connections_vec_.empty())
        {
            auto conn_uptr = std::move(connections_vec_.back());
            connections_vec_.pop_back();
            if (conn_uptr->IsValid())
            {
                return conn_uptr.release();
            }
            total_connections_.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    if (auto conn = CreateConnection())
    {
        return conn;
    }
    return nonstd::make_unexpected("failed to create connection for pool");
}

void ConnectionPool::Release(IpcConnection *conn, bool should_reconnect)
{
    if (!conn)
        return;

    std::lock_guard<std::mutex> lock(mutex_);

    if (should_reconnect || !conn->IsValid())
    {
        LOG_WARN("Releasing broken connection, reconnecting, server={}", server_uuid_);
        delete conn;
        total_connections_.fetch_sub(1, std::memory_order_relaxed);
    }
    else
    {
        connections_vec_.emplace_back(conn);
        LOG_DEBUG("Released connection to pool, server={}", server_uuid_);
    }

    acquire_cv_.notify_one();
}

const std::string &ConnectionPool::GetServerUuid() const
{ return server_uuid_; }

void ConnectionPool::Stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    connections_vec_.clear();
    total_connections_.store(0, std::memory_order_relaxed);
}

IpcConnection *ConnectionPool::CreateConnection()
{
    auto conn = std::make_unique<IpcConnection>();
    if (auto result = conn->Connect(server_uuid_, config_.connect_timeout_ms, config_.rw_timeout_ms); !result)
    {
        LOG_ERROR("Failed to connect new connection, server={}, error={}", server_uuid_, result.error());
        return nullptr;
    }
    total_connections_.fetch_add(1, std::memory_order_relaxed);
    LOG_DEBUG("Created and connected new connection, server={}", server_uuid_);
    return conn.release();
}
}// namespace tyke
