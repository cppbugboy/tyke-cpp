/**
 * @file ipc_client.h
 * @brief IPC客户端声明。提供同步/异步发送接口，支持明文传输通信。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 本模块提供IPC客户端功能，主要特性包括：
 * - 明文传输：数据以明文形式收发，无加密开销
 * - 大消息支持：自动分片发送和重组接收
 * - 连接池：支持连接复用，提高性能
 *
 * @example
 * @code
 * // 方式1：直接使用IpcConnection（需要手动管理连接）
 * tyke::IpcConnection conn;
 * auto result = conn.Connect("my_service", 3000);
 * if (result.has_value()) {
 *     conn.Write(data.data(), data.size(), 3000);
 *     conn.ReadLoop([](const std::vector<uint8_t>& data) -> bool {
 *         // 处理响应
 *         return false;  // 返回true停止读取
 *     }, 3000);
 *     conn.Close();
 * }
 *
 * // 方式2：使用静态方法（自动管理连接）
 * tyke::IpcClient::Send("my_service", request, [](const std::vector<uint8_t>& resp) -> bool {
 *     // 处理响应
 *     return true;  // 返回true停止读取
 * }, 3000);
 * @endcode
 */


#pragma once

#include <chrono>
#include <memory>
#include <string_view>

#include "tyke/common/tyke_def.h"
#include "ipc_def.h"

namespace tyke
{
    /**
     * @class IpcConnection
     * @brief IPC连接类，代表一个与服务端的连接。
     *
     * 该类提供完整的连接生命周期管理，包括连接建立、数据收发和连接关闭。
     * 连接建立后直接收发明文数据。
     *
     * @warning 线程安全：IpcConnection 实例**非线程安全**。同一连接不可被多个线程
     *          并发调用 Write/ReadLoop/Connect/Close。如需双工收发，
     *          请为读、写分别创建独立的 IpcConnection 实例。
     */
    class IpcConnection
    {
    public:
        /**
         * @brief 构造函数，创建未连接的实例。
         */
        IpcConnection();

        /**
         * @brief 析构函数，自动关闭连接并释放资源。
         */
        ~IpcConnection();

        /**
         * @brief 连接到IPC服务端。
         *
         * @param server_name 服务名称（与服务端启动时使用的名称一致）
         * @param timeout_ms 连接超时时间（毫秒），默认5秒
         * @param rw_timeout_ms 读写超时时间（毫秒），默认5秒
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         *
         * @note 可以在Close()后重新调用Connect()重连。
         */
        [[nodiscard]] BoolResult Connect(std::string_view server_name, uint32_t timeout_ms = kIpcDefaultTimeoutMs,
                                         uint32_t rw_timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 发送数据到服务端。
         *
         * @param data 数据指针
         * @param size 数据大小（字节）
         * @param timeout_ms 超时时间（毫秒），默认5秒
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         *
         * @note 数据以明文形式发送。
         * @note 大于64KB的数据会自动分片发送。
         * @note 该方法只发送数据，不等待响应。
         * @note 不可与同一实例的其他收发方法并发调用。
         */
        [[nodiscard]] BoolResult Write(const void* data, size_t size,
                                       uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 启动读取循环，接收服务端数据。
         *
         * @param callback 数据接收回调函数，参数为接收到的数据，
         *                 返回true表示停止读取，返回false表示继续读取
         * @param timeout_ms 单次读取超时时间（毫秒），默认5秒
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         *
         * @note 该方法会阻塞当前线程，直到回调返回true或发生错误。
         * @note 分片消息会自动重组后调用回调。
         * @note 不可与同一实例的其他收发方法并发调用。
         */
        [[nodiscard]] BoolResult ReadLoop(const ClientRecvDataCallback& callback,
                                          uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 关闭连接。
         *
         * 释放所有资源，连接可以重新Connect()。
         * 该方法可以安全地多次调用。
         */
        void Close();

        /**
         * @brief 检查连接是否有效。
         *
         * @return true 连接有效，可以收发数据
         * @return false 连接无效或已关闭
         */
        [[nodiscard]] bool IsValid() const;

    private:
        std::unique_ptr<class IClientConnectionImpl> impl_;
    };


    /**
     * @class IpcClient
     * @brief IPC客户端静态工具类，提供简化的发送接口。
     *
     * 该类提供静态方法，自动管理连接生命周期，适用于一次性请求场景。
     * 对于频繁通信的场景，建议使用IpcConnection以复用连接。
     */
    class IpcClient
    {
    public:
        IpcClient() = delete;

        /**
         * @brief 同步发送请求并接收响应。
         *
         * @param server_name 服务名称
         * @param request 请求数据
         * @param callback 响应接收回调函数
         * @param timeout_ms 超时时间（毫秒），默认5秒
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         *
         * @note 该方法会自动创建连接、发送请求、接收响应后关闭连接。
         */
        static BoolResult Send(std::string_view server_name, const std::vector<uint8_t>& request,
                               const ClientRecvDataCallback& callback, uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        /**
         * @brief 异步发送请求（不等待响应）。
         *
         * @param server_name 服务名称
         * @param request 请求数据
         * @param timeout_ms 超时时间（毫秒），默认5秒
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         *
         * @note 该方法发送数据后立即返回，不等待响应。
         */
        static BoolResult SendAsync(std::string_view server_name, const std::vector<uint8_t>& request,
                                    uint32_t timeout_ms = kIpcDefaultTimeoutMs);
    };
} // namespace tyke