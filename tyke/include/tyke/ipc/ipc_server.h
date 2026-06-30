/**
 * @file ipc_server.h
 * @brief IPC服务端声明。跨平台IPC服务器，支持Windows命名管道和Linux域套接字。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 本模块提供高性能的本地进程间通信(IPC)服务端实现，主要特性包括：
 * - 跨平台支持：Windows使用命名管道，Linux使用Unix域套接字
 * - 明文通信：数据以明文形式收发，无加密开销
 * - 高并发：Windows使用IOCP，Linux使用epoll
 * - 大消息支持：应用层分片机制，支持最大16MB消息
 *
 * @example
 * @code
 * // 创建并启动服务端
 * tyke::IpcServer server;
 * auto result = server.Start("my_service", [](tyke::ClientId id, 
 *     const std::vector<uint8_t>& data, auto send_cb) -> std::optional<uint32_t> {
 *     // 处理客户端请求
 *     send_cb(id, response_data);  // 发送响应
 *     return 0;  // 返回消费的字节数
 * });
 *
 * // 发送数据给特定客户端
 * server.SendToClient(client_id, data);
 *
 * // 停止服务
 * server.Stop();
 * @endcode
 */


#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "tyke/common/tyke_def.h"
#include "ipc_def.h"

namespace tyke
{
    /**
     * @class IpcServer
     * @brief IPC服务端类，提供跨平台的本地进程间通信服务端功能。
     *
     * 该类封装了平台相关的IPC实现细节，提供统一的API接口。
     * 使用PIMPL模式隐藏实现细节，确保ABI稳定性。
     */
    class IpcServer
    {
    public:
        /**
         * @brief 构造函数，初始化服务端实例。
         */
        IpcServer();

        /**
         * @brief 析构函数，自动停止服务并释放资源。
         */
        ~IpcServer();

        /**
         * @brief 启动IPC服务端。
         *
         * @param server_name 服务名称，用于客户端连接标识。
         *                    Windows: 创建命名管道 \\.\pipe\<server_name>
         *                    Linux: 创建Unix域套接字 @tyke_<server_name> (abstract namespace)
         * @param callback 数据接收回调函数，参数为：
         *                 - ClientId: 客户端标识
         *                 - const std::vector<uint8_t>&: 接收到的数据
         *                 - ServerSendDataCallback: 发送响应的回调函数
         *                 返回值：std::optional<uint32_t>，消费的字节数或nullopt表示数据不完整
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         *
         * @note 回调函数会在工作线程中被调用，需要确保线程安全。
         * @note 服务端启动后会在后台监听连接，无需手动管理线程。
         */
        [[nodiscard]] BoolResult Start(std::string_view server_name, ServerRecvDataCallback callback) const;

        /**
         * @brief 停止IPC服务端。
         *
         * 关闭所有客户端连接，停止监听，释放资源。
         * 该方法可以安全地多次调用。
         */
        void Stop() const;

        /**
         * @brief 向指定客户端发送数据。
         *
         * @param id 目标客户端ID（由回调函数提供）
         * @param data 要发送的数据
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         *
         * @note 数据以明文形式发送。
         * @note 大于64KB的数据会自动分片发送。
         */
        [[nodiscard]] BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data) const;

    private:
        std::shared_ptr<class IServerImpl> impl_;
    };

    /**
     * @brief 获取全局IPC服务端单例。
     *
     * @return IpcServer& 全局服务端实例引用
     *
     * @note 适用于单服务场景，简化代码。
     */
    inline IpcServer& GetGlobalIpcServer()
    {
        static IpcServer instance;
        return instance;
    }
} // namespace tyke