/**
 * @file connection_pool.cpp
 * @brief IPC 连接池实现。管理到指定服务端的连接复用，支持最大连接数限制、空闲连接复用、健康检查和优雅关闭。
 *
 * 核心流程：
 * - Acquire：从 idle 队列取连接 -> 创建新连接（不超过 max） -> 阻塞等待归还
 * - Release：归还健康连接到 idle 队列，或标记重连/销毁
 *
 * 使用 CAS 原子操作和条件变量实现线程安全。
 *
 * @author Nick
 * @date 2026/04/20
 */

#include "tyke/ipc/connection_pool.h"

#include "tyke/common/log_def.h"

namespace tyke
{
    /** @brief PooledConnection 自定义删除器：归还连接到池而非销毁。 */
    void ConnectionDeleter::operator()(IpcConnection* conn) const
    {
        if (pool && conn)
        {
            pool->Release(conn);
        }
    }

    /** @brief 构造连接池。 */
    ConnectionPool::ConnectionPool(const std::string_view server_uuid, const ConnectionPoolConfig& config)
        : server_uuid_(server_uuid), config_(config)
    {
        LOG_INFO("Connection pool created, server={}", server_uuid_);
    }

    /** @brief 析构时停止池。 */
    ConnectionPool::~ConnectionPool()
    {
        Stop();
    }

    /**
     * @brief 从池中获取一个连接。
     *
     * 获取顺序：
     * 1. 从 idle 队列取连接（若有效则返回，无效则销毁）
     * 2. 若未达最大连接数，创建新连接
     * 3. 若已达上限，阻塞等待（最长 acquire_timeout_ms）直到有连接归还
     *
     * @return PooledConnection（RAII 包装，析构时自动归还）或错误。
     *
     * @note 在锁内递增 total_connections_ 预留配额，避免"检查-创建" TOCTOU 竞态。
     */
    TResult<PooledConnection> ConnectionPool::Acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (stopped_.load(std::memory_order_acquire))
        {
            return nonstd::make_unexpected("connection pool is stopped");
        }

        while (true)
        {
            // 1. 尝试从 idle 队列获取（idle 连接已计入 total_connections_）
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

            // 2. 检查是否可创建新连接（锁内递增预留配额）
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
        }
    }

    /**
     * @brief 归还连接到池。
     *
     * @param conn 连接指针（接管所有权）
     * @param should_reconnect 若为 true 或连接无效，销毁连接而非归还到 idle 队列
     */
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

    /** @brief 停止连接池：唤醒所有等待者，关闭并清空所有连接。 */
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

    /**
     * @brief 创建并连接到服务端的新连接。
     *
     * @note total_connections_ 的递增已由 Acquire 在锁内完成（预留配额），此处不重复递增。
     * @return 成功返回连接裸指针（调用方接管所有权），失败返回 nullptr。
     */
    IpcConnection* ConnectionPool::CreateConnection()
    {
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
