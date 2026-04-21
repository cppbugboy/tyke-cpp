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
    ConnectionPool::ConnectionPool(std::string_view server_uuid, const ConnectionPoolConfig& config)
        : server_uuid_(server_uuid), config_(config)
    {
        LOG_INFO("Connection pool created, server={}, max={}, min_idle={}",
                 server_uuid_, config_.max_connections, config_.min_idle_connections);

        cleanup_thread_ = std::thread(&ConnectionPool::CleanupThreadFunc, this);
    }

    ConnectionPool::~ConnectionPool()
    {
        Stop();
    }

    Result<IpcConnection*> ConnectionPool::Acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        while (!idle_connections_.empty())
        {
            IpcConnection* conn = idle_connections_.back();
            idle_connections_.pop_back();

            if (conn->IsValid())
            {
                ++active_count_;
                conn->UpdateLastUsedTime();
                LOG_DEBUG("Acquired idle connection from pool, server={}, idle={}, active={}",
                          server_uuid_, idle_connections_.size(), active_count_);
                return conn;
            }
            else
            {
                LOG_WARN("Idle connection invalid, destroying, server={}", server_uuid_);
                delete conn;
            }
        }

        if (active_count_ < config_.max_connections)
        {
            IpcConnection* conn = CreateConnection();
            if (conn)
            {
                ++active_count_;
                LOG_DEBUG("Created new connection in pool, server={}, idle={}, active={}",
                          server_uuid_, idle_connections_.size(), active_count_);
                return conn;
            }
            return nonstd::make_unexpected("failed to create connection for pool");
        }

        bool signaled = acquire_cv_.wait_for(
            lock,
            std::chrono::milliseconds(config_.acquire_timeout_ms),
            [this]
            {
                return !idle_connections_.empty() || active_count_ < config_.max_connections ||
                       stopped_.load();
            });

        if (signaled && !stopped_.load())
        {
            while (!idle_connections_.empty())
            {
                IpcConnection* conn = idle_connections_.back();
                idle_connections_.pop_back();
                if (conn->IsValid())
                {
                    ++active_count_;
                    conn->UpdateLastUsedTime();
                    return conn;
                }
                else
                {
                    delete conn;
                }
            }

            if (active_count_ < config_.max_connections)
            {
                IpcConnection* conn = CreateConnection();
                if (conn)
                {
                    ++active_count_;
                    return conn;
                }
            }
        }

        return nonstd::make_unexpected("acquire connection timeout, server=" + server_uuid_);
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

            size_t total = idle_connections_.size() + active_count_;
            if (total == 0 || total - 1 < config_.min_idle_connections)
            {
                IpcConnection* new_conn = CreateConnection();
                if (new_conn)
                {
                    idle_connections_.push_back(new_conn);
                    LOG_DEBUG("Created replacement connection, server={}", server_uuid_);
                }
            }
        }
        else
        {
            conn->UpdateLastUsedTime();
            idle_connections_.push_back(conn);
            LOG_DEBUG("Released connection to pool, server={}, idle={}, active={}",
                      server_uuid_, idle_connections_.size(), active_count_ - 1);
        }

        --active_count_;
        acquire_cv_.notify_one();
    }

    size_t ConnectionPool::GetIdleCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return idle_connections_.size();
    }

    size_t ConnectionPool::GetActiveCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_count_;
    }

    const std::string& ConnectionPool::GetServerUuid() const
    {
        return server_uuid_;
    }

    void ConnectionPool::Stop()
    {
        if (stopped_.exchange(true))
            return;

        acquire_cv_.notify_all();

        if (cleanup_thread_.joinable())
        {
            cleanup_thread_.join();
        }

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto* conn : idle_connections_)
        {
            delete conn;
        }
        idle_connections_.clear();
        active_count_ = 0;

        LOG_INFO("Connection pool stopped, server={}", server_uuid_);
    }

    IpcConnection* ConnectionPool::CreateConnection()
    {
        IpcConnection* conn = new IpcConnection();
        auto result = conn->Connect(server_uuid_, config_.connect_timeout_ms, config_.rw_timeout_ms);
        if (!result)
        {
            LOG_ERROR("Failed to connect new connection, server={}, error={}", server_uuid_, result.error());
            delete conn;
            return nullptr;
        }
        LOG_DEBUG("Created and connected new connection, server={}", server_uuid_);
        return conn;
    }

    void ConnectionPool::CleanupIdleConnections()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(config_.idle_timeout_ms);

        auto it = idle_connections_.begin();
        while (it != idle_connections_.end())
        {
            bool should_remove = false;

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - (*it)->GetLastUsedTime());
            if (elapsed > timeout)
            {
                should_remove = true;
                LOG_DEBUG("Idle connection timeout, removing, server={}", server_uuid_);
            }

            if (!(*it)->IsValid())
            {
                should_remove = true;
                LOG_DEBUG("Idle connection invalid, removing, server={}", server_uuid_);
            }

            size_t remaining = idle_connections_.size() + active_count_;
            if (should_remove && remaining <= config_.min_idle_connections)
            {
                ++it;
                continue;
            }

            if (should_remove)
            {
                delete *it;
                it = idle_connections_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void ConnectionPool::CleanupThreadFunc()
    {
        while (!stopped_.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.idle_timeout_ms / 2));
            if (stopped_.load())
                break;
            CleanupIdleConnections();
        }
    }
}
