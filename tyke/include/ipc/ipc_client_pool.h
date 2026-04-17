/**
 * @file ipc_client_pool.h
 * @brief IPC客户端连接池
 * @author Nick
 * @date 2026/04/17
 *
 * IpcClientPool管理IPC客户端连接池，提供连接复用和空闲连接清理功能。
 */

#ifndef IPC_CLIENT_POOL_H_
#define IPC_CLIENT_POOL_H_

#include "ipc_client.h"
#include "common/tyke_result.h"
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>

namespace tyke
{
    /**
     * @brief IPC客户端连接池类
     *
     * 管理IPC客户端连接池，支持连接复用、最大连接数限制和空闲连接清理。
     *
     * 使用示例：
     * @code
     *   IpcClientPool pool("my-server", 4);
     *   pool.Send(request_data, [](const std::vector<uint8_t>& response) {
     *       // 处理响应
     *       return true;
     *   });
     * @endcode
     */
    class IpcClientPool
    {
    public:
        /**
         * @brief 构造函数
         * @param server_name 服务器名称
         * @param max_connections 最大连接数
         * @param idle_timeout_ms 空闲连接超时时间（毫秒）
         * @param connect_timeout_ms 连接超时时间（毫秒）
         * @param read_write_timeout_ms 读写超时时间（毫秒）
         */
        explicit IpcClientPool(std::string server_name,
                               size_t max_connections = kIpcDefaultMaxConnections,
                               uint32_t idle_timeout_ms = kIpcDefaultIdleTimeoutMs,
                               uint32_t connect_timeout_ms = kIpcDefaultTimeoutMs,
                               uint32_t read_write_timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 析构函数
         */
        ~IpcClientPool();

        // 禁止拷贝
        IpcClientPool(const IpcClientPool&) = delete;
        IpcClientPool& operator=(const IpcClientPool&) = delete;

        /**
         * @brief 同步发送请求
         * @param request 请求数据
         * @param callback 接收响应的回调函数
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult Send(const std::vector<uint8_t>& request, const ClientRecvDataCallback& callback);

        /**
         * @brief 异步发送请求
         * @param request 请求数据
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult SendAsync(const std::vector<uint8_t>& request);

        /**
         * @brief 清理空闲连接
         *
         * 关闭超过空闲超时时间的连接。
         */
        void CleanupIdleConnections();

    private:
        /**
         * @brief 获取连接
         * @return 连接指针，无可用连接返回nullptr
         */
        std::unique_ptr<IpcConnection> Acquire();

        /**
         * @brief 释放连接
         * @param conn 连接指针
         * @param valid 连接是否有效
         */
        void Release(std::unique_ptr<IpcConnection> conn, bool valid);

        std::string server_name_;                       ///< 服务器名称
        size_t max_connections_;                        ///< 最大连接数
        uint32_t idle_timeout_ms_;                      ///< 空闲超时时间
        uint32_t connect_timeout_ms_;                   ///< 连接超时时间
        uint32_t read_write_timeout_ms_;                ///< 读写超时时间

        std::mutex mutex_;                              ///< 互斥锁
        std::condition_variable cv_;                    ///< 条件变量
        std::vector<std::unique_ptr<IpcConnection>> idle_conns_;  ///< 空闲连接池
        size_t active_count_;                           ///< 活跃连接数
        bool stopped_;                                  ///< 是否已停止
    };
} // namespace tyke

#endif // IPC_CLIENT_POOL_H_
