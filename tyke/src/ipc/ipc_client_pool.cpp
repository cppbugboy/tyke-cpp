/**
 * @file ipc_client_pool.cpp
 * @brief IPC客户端连接池实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现IpcClientPool类的具体逻辑，管理IPC客户端连接池。
 */

#include "ipc/ipc_client_pool.h"
#include "common/log_def.h"

namespace tyke
{
    /**
     * @brief 构造函数
     * @param server_name 服务器名称
     * @param max_connections 最大连接数
     * @param idle_timeout_ms 空闲连接超时时间（毫秒）
     * @param connect_timeout_ms 连接超时时间（毫秒）
     * @param read_write_timeout_ms 读写超时时间（毫秒）
     */
    IpcClientPool::IpcClientPool(std::string server_name, size_t max_connections, uint32_t idle_timeout_ms,
                                 uint32_t connect_timeout_ms, uint32_t read_write_timeout_ms)
        : server_name_(std::move(server_name)), max_connections_(max_connections),
          idle_timeout_ms_(idle_timeout_ms), connect_timeout_ms_(connect_timeout_ms),
          read_write_timeout_ms_(read_write_timeout_ms), active_count_(0), stopped_(false)
    {
        LOG_DEBUG("IpcClientPool created: server={}, max_connections={}", server_name_, max_connections_);
    }

    /**
     * @brief 析构函数
     */
    IpcClientPool::~IpcClientPool()
    {
        LOG_DEBUG("IpcClientPool destroying: server={}", server_name_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
            idle_conns_.clear();
        }
        cv_.notify_all();
        LOG_DEBUG("IpcClientPool destroyed");
    }

    /**
     * @brief 同步发送请求
     * @param request 请求数据
     * @param callback 接收响应的回调函数
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult IpcClientPool::Send(const std::vector<uint8_t>& request, const ClientRecvDataCallback& callback)
    {
        LOG_DEBUG("IpcClientPool::Send: request_size={} bytes", request.size());

        auto conn = Acquire();
        if (!conn)
        {
            LOG_ERROR("IpcClientPool::Send failed to acquire connection");
            return nonstd::make_unexpected("pool send: failed to acquire connection");
        }

        auto write_result = conn->WriteEncrypted(request.data(), request.size(), read_write_timeout_ms_);
        if (!write_result)
        {
            LOG_ERROR("IpcClientPool::Send write failed: {}", write_result.error());
            Release(std::move(conn), false);
            return nonstd::make_unexpected("pool send: " + write_result.error());
        }

        auto read_result = conn->ReadLoop(callback, read_write_timeout_ms_);
        if (!read_result)
        {
            LOG_ERROR("IpcClientPool::Send read failed: {}", read_result.error());
            Release(std::move(conn), false);
            return nonstd::make_unexpected("pool send: " + read_result.error());
        }

        Release(std::move(conn), true);
        LOG_DEBUG("IpcClientPool::Send completed successfully");
        return true;
    }

    /**
     * @brief 异步发送请求
     * @param request 请求数据
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult IpcClientPool::SendAsync(const std::vector<uint8_t>& request)
    {
        LOG_DEBUG("IpcClientPool::SendAsync: request_size={} bytes", request.size());

        auto conn = Acquire();
        if (!conn)
        {
            LOG_ERROR("IpcClientPool::SendAsync failed to acquire connection");
            return nonstd::make_unexpected("pool send async: failed to acquire connection");
        }

        auto write_result = conn->WriteEncrypted(request.data(), request.size(), read_write_timeout_ms_);
        if (!write_result)
        {
            LOG_ERROR("IpcClientPool::SendAsync write failed: {}", write_result.error());
            Release(std::move(conn), false);
            return nonstd::make_unexpected("pool send async: " + write_result.error());
        }

        Release(std::move(conn), true);
        LOG_DEBUG("IpcClientPool::SendAsync completed successfully");
        return true;
    }

    /**
     * @brief 清理空闲连接
     */
    void IpcClientPool::CleanupIdleConnections()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        size_t cleaned = 0;
        for (auto it = idle_conns_.begin(); it != idle_conns_.end();)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - (*it)->GetLastUsedTime()).count() >=
                idle_timeout_ms_)
            {
                it = idle_conns_.erase(it);
                --active_count_;
                ++cleaned;
            }
            else
                ++it;
        }
        if (cleaned > 0)
        {
            LOG_DEBUG("Cleaned {} idle connections", cleaned);
        }
    }

    /**
     * @brief 获取连接
     * @return 连接指针，无可用连接返回nullptr
     */
    std::unique_ptr<IpcConnection> IpcClientPool::Acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // 清理过期的空闲连接
        auto now = std::chrono::steady_clock::now();
        for (auto it = idle_conns_.begin(); it != idle_conns_.end();)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - (*it)->GetLastUsedTime()).count() >=
                idle_timeout_ms_)
            {
                it = idle_conns_.erase(it);
                --active_count_;
            }
            else
                ++it;
        }

        // 等待可用连接或连接槽位
        bool success = cv_.wait_for(lock, std::chrono::milliseconds(connect_timeout_ms_), [this]()
        {
            return stopped_ || !idle_conns_.empty() || active_count_ < max_connections_;
        });

        if (stopped_ || !success)
        {
            LOG_WARN("Acquire connection failed: stopped={}, timeout", stopped_);
            return nullptr;
        }

        // 从空闲池获取连接
        if (!idle_conns_.empty())
        {
            auto conn = std::move(idle_conns_.back());
            idle_conns_.pop_back();
            LOG_DEBUG("Acquired connection from idle pool");
            return conn;
        }

        // 创建新连接
        if (active_count_ < max_connections_)
        {
            ++active_count_;
            lock.unlock();
            auto conn = std::unique_ptr<IpcConnection>(new IpcConnection());
            if (!conn->Connect(server_name_, connect_timeout_ms_, read_write_timeout_ms_) || !conn->IsValid())
            {
                LOG_ERROR("Failed to create new connection to {}", server_name_);
                lock.lock();
                --active_count_;
                cv_.notify_one();
                return nullptr;
            }
            LOG_DEBUG("Created new connection, active_count={}", active_count_);
            return conn;
        }

        LOG_WARN("No available connection slot");
        return nullptr;
    }

    /**
     * @brief 释放连接
     * @param conn 连接指针
     * @param valid 连接是否有效
     */
    void IpcClientPool::Release(std::unique_ptr<IpcConnection> conn, bool valid)
    {
        if (!conn)
            return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
            return;
        if (valid)
        {
            conn->UpdateLastUsedTime();
            idle_conns_.push_back(std::move(conn));
            LOG_DEBUG("Connection returned to idle pool");
        }
        else
        {
            conn->Close();
            conn.reset();
            --active_count_;
            LOG_DEBUG("Invalid connection closed, active_count={}", active_count_);
        }
        cv_.notify_one();
    }
} // namespace tyke
