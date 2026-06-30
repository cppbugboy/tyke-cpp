#include "tyke/ipc/connection_pool.h"

#include "tyke/common/log_def.h"

namespace tyke
{
    void ConnectionDeleter::operator()(IpcConnection* conn) const
    {
        if (pool && conn)
        {
            pool->Release(conn);
        }
    }

    ConnectionPool::ConnectionPool(const std::string_view server_uuid, const ConnectionPoolConfig& config)
        : server_uuid_(server_uuid), config_(config)
    {
        LOG_INFO("Connection pool created, server={}", server_uuid_);
    }

    ConnectionPool::~ConnectionPool()
    {
        Stop();
    }

    TResult<PooledConnection> ConnectionPool::Acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (stopped_.load(std::memory_order_acquire))
        {
            return nonstd::make_unexpected("connection pool is stopped");
        }

        while (true)
        {
            // 1. 尝试从 idle 队列获取（idle 连接已计入 total_connections_，无需递增）
            while (!connections_vec_.empty())
            {
                auto conn_uptr = std::move(connections_vec_.back());
                connections_vec_.pop_back();
                lock.unlock();

                if (conn_uptr->IsValid())
                {
                    IpcConnection* raw = conn_uptr.release();
                    return PooledConnection(raw, ConnectionDeleter{this});
                }

                LOG_WARN("Idle connection invalid, destroying, server={}", server_uuid_);
                conn_uptr.reset();
                total_connections_.fetch_sub(1, std::memory_order_relaxed);
                lock.lock();
            }

            // 2. 检查是否可创建新连接。
            //    关键：在锁内递增 total_connections_ 预留配额，避免"检查-创建"竞态导致超限。
            if (config_.max_connections == 0 ||
                total_connections_.load(std::memory_order_relaxed) < config_.max_connections)
            {
                total_connections_.fetch_add(1, std::memory_order_relaxed);
                lock.unlock();

                IpcConnection* conn = CreateConnection();
                if (conn)
                {
                    return PooledConnection(conn, ConnectionDeleter{this});
                }
                // 创建失败，归还配额
                lock.lock();
                total_connections_.fetch_sub(1, std::memory_order_relaxed);
                return nonstd::make_unexpected("failed to create connection for pool");
            }

            // 3. 达到上限，等待归还或停止
            if (!acquire_cv_.wait_for(lock, std::chrono::milliseconds(config_.acquire_timeout_ms),
                                      [this]
                                      {
                                          return stopped_.load(std::memory_order_acquire) ||
                                              !connections_vec_.empty() ||
                                              total_connections_.load(std::memory_order_relaxed) <
                                              config_.max_connections;
                                      }))
            {
                return nonstd::make_unexpected(
                    "connection pool exhausted, max=" + std::to_string(config_.max_connections));
            }

            if (stopped_.load(std::memory_order_acquire))
            {
                return nonstd::make_unexpected("connection pool is stopped");
            }
            // 循环回到步骤 1 重新评估 idle / max
        }
    }

    void ConnectionPool::Release(IpcConnection* conn, bool should_reconnect)
    {
        if (!conn)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        std::unique_ptr<IpcConnection> guard(conn);

        if (stopped_.load(std::memory_order_acquire))
        {
            total_connections_.fetch_sub(1, std::memory_order_relaxed);
            return;
        }

        if (should_reconnect || !conn->IsValid())
        {
            LOG_WARN("Releasing broken connection, reconnecting, server={}", server_uuid_);
            total_connections_.fetch_sub(1, std::memory_order_relaxed);
        }
        else
        {
            connections_vec_.emplace_back(guard.release());
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
        if (stopped_.exchange(true, std::memory_order_acq_rel))
            return;

        acquire_cv_.notify_all();

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& conn : connections_vec_)
        {
            conn->Close();
        }
        connections_vec_.clear();
        total_connections_.store(0, std::memory_order_relaxed);
    }

    IpcConnection* ConnectionPool::CreateConnection()
    {
        // 注意：total_connections_ 的递增已由 Acquire 在锁内完成（预留配额），
        // 此处不再递增，避免与 Acquire 的配额预留重复计数。
        auto conn = std::make_unique<IpcConnection>();
        if (auto result = conn->Connect(server_uuid_, config_.connect_timeout_ms, config_.rw_timeout_ms); !result)
        {
            LOG_ERROR("Failed to connect new connection, server={}, error={}", server_uuid_, result.error());
            return nullptr;
        }
        LOG_DEBUG("Created and connected new connection, server={}", server_uuid_);
        return conn.release();
    }
} // namespace tyke
