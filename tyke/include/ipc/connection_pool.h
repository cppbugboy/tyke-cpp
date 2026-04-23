/**
 * @file connection_pool.h
 * @brief 连接池声明。管理IpcConnection的获取、归还、健康检查和空闲清理。
 * @author Nick
 * @date 2026/04/20
 *
 * 线程安全的连接池，支持最大连接数限制、最小空闲连接保持、
 * 空闲超时清理和连接健康检查。
 */


#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ipc_client.h"
#include "common/tyke_def.h"

namespace tyke
{
    // struct ConnectionPoolConfig
    // {
    //     size_t max_connections = kIpcDefaultMaxConnections;
    //     size_t min_idle_connections = 1;
    //     uint32_t idle_timeout_ms = kIpcDefaultIdleTimeoutMs;
    //     uint32_t connect_timeout_ms = kIpcDefaultTimeoutMs;
    //     uint32_t rw_timeout_ms = kIpcDefaultTimeoutMs;
    //     uint32_t acquire_timeout_ms = 3000;
    // };

    class ConnectionPool
    {
    public:
        explicit ConnectionPool(std::string_view server_uuid);

        ~ConnectionPool();

        ConnectionPool(const ConnectionPool&) = delete;
        ConnectionPool& operator=(const ConnectionPool&) = delete;

        TResult<IpcConnection*> Acquire();

        void Release(IpcConnection* conn, bool should_reconnect = false);

        const std::string& GetServerUuid() const;

        void Stop();

    private:
        IpcConnection* CreateConnection();

        std::string server_uuid_;

        std::vector<IpcConnection*> connections_vec_;

        mutable std::mutex mutex_;
        std::condition_variable acquire_cv_;
    };
}