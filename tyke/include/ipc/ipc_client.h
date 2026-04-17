/**
 * @file ipc_client.h
 * @brief IPC客户端
 * @author Nick
 * @date 2026/04/17
 *
 * IpcConnection和IpcClient提供进程间通信的客户端功能，支持加密通信和连接池管理。
 */

#ifndef IPC_CLIENT_H_
#define IPC_CLIENT_H_

#include "ipc_types.h"
#include "common/tyke_result.h"
#include <memory>
#include <chrono>

namespace tyke
{
    /**
     * @brief IPC连接类
     *
     * 表示一个到服务器的IPC连接，支持加密通信。
     * 提供连接、读写和关闭功能。
     */
    class IpcConnection
    {
    public:
        /**
         * @brief 构造函数
         */
        IpcConnection();

        /**
         * @brief 析构函数
         */
        ~IpcConnection();

        /**
         * @brief 连接到服务器
         * @param server_name 服务器名称
         * @param timeout_ms 连接超时时间（毫秒）
         * @param rw_timeout_ms 读写超时时间（毫秒）
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult Connect(const std::string& server_name, uint32_t timeout_ms = kIpcDefaultTimeoutMs,
                     uint32_t rw_timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 加密写入数据
         * @param data 数据指针
         * @param size 数据大小
         * @param timeout_ms 超时时间（毫秒）
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult WriteEncrypted(const void* data, size_t size, uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 读取循环
         * @param callback 接收数据的回调函数
         * @param timeout_ms 超时时间（毫秒）
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 关闭连接
         */
        void Close();

        /**
         * @brief 检查连接是否有效
         * @return 有效返回true，无效返回false
         */
        bool IsValid() const;

        /**
         * @brief 更新最后使用时间
         */
        void UpdateLastUsedTime() { last_used_ = std::chrono::steady_clock::now(); }

        /**
         * @brief 获取最后使用时间
         * @return 最后使用时间点
         */
        std::chrono::steady_clock::time_point GetLastUsedTime() const { return last_used_; }

    private:
        std::unique_ptr<class IClientConnectionImpl> impl_;  ///< 平台相关的实现
        std::chrono::steady_clock::time_point last_used_;    ///< 最后使用时间
    };

    /**
     * @brief IPC客户端类
     *
     * 提供静态方法实现同步和异步的IPC通信。
     */
    class IpcClient
    {
    public:
        IpcClient() = delete;

        /**
         * @brief 同步发送请求
         * @param server_name 服务器名称
         * @param request 请求数据
         * @param callback 接收响应的回调函数
         * @param timeout_ms 超时时间（毫秒）
         * @return 成功返回true，失败返回错误信息
         */
        static BoolResult Send(const std::string& server_name, const std::vector<uint8_t>& request,
                         const ClientRecvDataCallback& callback, uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 异步发送请求
         * @param server_name 服务器名称
         * @param request 请求数据
         * @param timeout_ms 超时时间（毫秒）
         * @return 成功返回true，失败返回错误信息
         */
        static BoolResult SendAsync(const std::string& server_name, const std::vector<uint8_t>& request,
                              uint32_t timeout_ms = kIpcDefaultTimeoutMs);
    };
} // namespace tyke

#endif // IPC_CLIENT_H_
