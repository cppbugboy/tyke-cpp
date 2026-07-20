/**
 * @file connection_pool.h
 * @brief 连接池声明。管理IpcConnection的获取、归还、健康检查和空闲清理。
 * @author Nick
 * @date 2026/04/20
 *
 * @details
 * 线程安全的IPC连接池实现，主要特性包括：
 * - 最大连接数限制，防止资源耗尽
 * - 最小空闲连接保持，降低连接建立延迟
 * - 空闲超时自动清理，回收不活跃连接
 * - 连接健康检查，剔除已断开的连接
 * - RAII风格的PooledConnection，自动归还连接
 *
 * @note 连接池本身是线程安全的，但单个IpcConnection实例不是。
 *       获取连接后，同一连接不可被多线程并发使用。
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "tyke/common/tyke_def.h"
#include "ipc_client.h"

namespace tyke
{
    class ConnectionPool;

    /**
     * @brief 连接池配置参数
     *
     * 控制连接池的容量、空闲策略和超时行为。
     * 所有超时值单位均为毫秒。
     */
    struct ConnectionPoolConfig
    {
        size_t max_connections = kIpcDefaultMaxConnections; ///< 最大同时连接数
        size_t min_idle_connections = 1; ///< 保持的最小空闲连接数
        uint32_t idle_timeout_ms = kIpcDefaultIdleTimeoutMs; ///< 空闲连接超时时间（毫秒）
        uint32_t connect_timeout_ms = kIpcDefaultTimeoutMs; ///< 新连接建立超时（毫秒）
        uint32_t rw_timeout_ms = kIpcDefaultTimeoutMs; ///< 读写操作超时（毫秒）
        uint32_t acquire_timeout_ms = 3000; ///< Acquire()等待可用连接的超时（毫秒）
    };

    /**
     * @brief 连接归还器（自定义删除器）
     *
     * 作为PooledConnection的删除器，在unique_ptr析构时自动将
     * IpcConnection归还到连接池而不是删除。
     */
    struct ConnectionDeleter
    {
        ConnectionPool* pool = nullptr; ///< 所属连接池指针
        void operator()(IpcConnection* conn) const;
    };

    /**
     * @brief 池化连接类型别名
     *
     * unique_ptr包装的IpcConnection，使用ConnectionDeleter作为删除器，
     * 离开作用域时自动归还到连接池。
     */
    using PooledConnection = std::unique_ptr<IpcConnection, ConnectionDeleter>;

    /**
     * @brief IPC连接池类
     *
     * 管理一组到特定服务的IpcConnection实例，支持连接的获取、归还和生命周期管理。
     * 线程安全，可在多线程环境中共享使用。
     */
    class ConnectionPool
    {
    public:
        /**
         * @brief 构造函数
         * @param server_uuid 目标服务端UUID，用于连接路由
         * @param config 连接池配置，使用默认参数时采用预设的合理值
         */
        explicit ConnectionPool(std::string_view server_uuid,
                                const ConnectionPoolConfig& config = ConnectionPoolConfig{});

        /**
         * @brief 析构函数，自动调用Stop()清理所有连接
         */
        ~ConnectionPool();

        ConnectionPool(const ConnectionPool&) = delete;
        ConnectionPool& operator=(const ConnectionPool&) = delete;

        /**
         * @brief 从池中获取一个可用连接
         *
         * 优先从空闲连接中获取，若无空闲连接且未达上限则创建新连接。
         * 如果连接数已达上限，阻塞等待直到有连接归还或超时。
         *
         * @return TResult<PooledConnection> 成功返回池化连接，失败返回错误信息
         * @note 返回的PooledConnection离开作用域时自动归还到池中
         */
        [[nodiscard]] TResult<PooledConnection> Acquire();

        /**
         * @brief 归还连接到池中
         * @param conn 要归还的连接指针（由PooledConnection的删除器自动调用）
         * @param should_reconnect 是否在归还前尝试重连（用于断线恢复）
         */
        void Release(IpcConnection* conn, bool should_reconnect = false);

        /**
         * @brief 获取连接池对应的服务端UUID
         * @return 服务端UUID字符串引用
         */
        [[nodiscard]] const std::string& GetServerUuid() const;

        /**
         * @brief 停止连接池，关闭所有连接并拒绝新的获取请求
         */
        void Stop();

    private:
        /**
         * @brief 创建并连接到服务端的新IpcConnection实例
         * @return 已连接的IpcConnection指针，失败返回nullptr
         */
        IpcConnection* CreateConnection();

        std::string server_uuid_; ///< 目标服务端UUID
        ConnectionPoolConfig config_; ///< 连接池配置

        std::vector<std::unique_ptr<IpcConnection>> connections_vec_; ///< 所有连接实例
        std::atomic<size_t> total_connections_{0}; ///< 当前总连接数
        std::atomic<bool> stopped_{false}; ///< 停止标志

        mutable std::mutex mutex_; ///< 保护连接池状态的互斥锁
        std::condition_variable acquire_cv_; ///< Acquire()等待的条件变量
    };
} // namespace tyke
