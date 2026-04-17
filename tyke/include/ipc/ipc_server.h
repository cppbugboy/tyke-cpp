/**
 * @file ipc_server.h
 * @brief IPC服务器
 * @author Nick
 * @date 2026/04/17
 *
 * IpcServer提供进程间通信的服务器端功能，支持多客户端连接和数据收发。
 * 使用平台相关的实现（Windows命名管道/Linux Unix域套接字）。
 */

#ifndef IPC_SERVER_H_
#define IPC_SERVER_H_

#include "ipc_types.h"
#include "common/tyke_result.h"
#include <memory>
#include <string>

namespace tyke
{
    /**
     * @brief IPC服务器类
     *
     * 提供进程间通信的服务器端功能，支持多客户端连接和数据收发。
     * 使用PIMPL模式隐藏平台相关的实现细节。
     *
     * 使用示例：
     * @code
     *   IpcServer server;
     *   auto result = server.Start("my-server", [](ClientId id, const std::vector<uint8_t>& data, auto send_cb) {
     *       // 处理接收到的数据
     *       return data.size();
     *   });
     *   server.SendToClient(client_id, response_data);
     *   server.Stop();
     * @endcode
     */
    class IpcServer
    {
    public:
        /**
         * @brief 构造函数
         */
        IpcServer();

        /**
         * @brief 析构函数
         */
        ~IpcServer();

        // 禁止拷贝
        IpcServer(const IpcServer&) = delete;
        IpcServer& operator=(const IpcServer&) = delete;

        /**
         * @brief 启动IPC服务器
         * @param server_name 服务器名称（用于标识IPC端点）
         * @param callback 接收数据的回调函数
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult Start(const std::string& server_name, ServerRecvDataCallback callback);

        /**
         * @brief 停止IPC服务器
         *
         * 关闭所有客户端连接并释放资源。
         */
        void Stop();

        /**
         * @brief 向指定客户端发送数据
         * @param id 客户端标识
         * @param data 待发送的数据
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data);

    private:
        std::unique_ptr<class IServerImpl> impl_;  ///< 平台相关的实现
    };
} // namespace tyke

#endif // IPC_SERVER_H_
