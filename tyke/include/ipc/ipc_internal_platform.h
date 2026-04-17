/**
 * @file ipc_internal_platform.h
 * @brief IPC平台接口定义
 * @author Nick
 * @date 2026/04/17
 *
 * 定义IPC模块的平台相关接口，用于实现跨平台的IPC通信。
 */

#ifndef IPC_INTERNAL_PLATFORM_H_
#define IPC_INTERNAL_PLATFORM_H_

#include "ipc_types.h"
#include "common/tyke_result.h"
#include <memory>

namespace tyke
{
    /**
     * @brief 客户端连接实现接口
     *
     * 定义客户端连接的平台相关操作接口。
     */
    class IClientConnectionImpl
    {
    public:
        virtual ~IClientConnectionImpl() = default;

        /**
         * @brief 连接到服务器
         * @param server_name 服务器名称
         * @param timeout_ms 连接超时时间（毫秒）
         * @param rw_timeout_ms 读写超时时间（毫秒）
         * @return 成功返回true，失败返回错误信息
         */
        virtual BoolResult Connect(const std::string& server_name, uint32_t timeout_ms, uint32_t rw_timeout_ms) = 0;

        /**
         * @brief 加密写入数据
         * @param data 数据指针
         * @param size 数据大小
         * @param timeout_ms 超时时间（毫秒）
         * @return 成功返回true，失败返回错误信息
         */
        virtual BoolResult WriteEncrypted(const void* data, size_t size, uint32_t timeout_ms) = 0;

        /**
         * @brief 读取循环
         * @param callback 接收数据的回调函数
         * @param timeout_ms 超时时间（毫秒）
         * @return 成功返回true，失败返回错误信息
         */
        virtual BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms) = 0;

        /**
         * @brief 关闭连接
         */
        virtual void Close() = 0;

        /**
         * @brief 检查连接是否有效
         * @return 有效返回true，无效返回false
         */
        virtual bool IsValid() const = 0;
    };

    /**
     * @brief 服务器实现接口
     *
     * 定义服务器的平台相关操作接口。
     */
    class IServerImpl
    {
    public:
        virtual ~IServerImpl() = default;

        /**
         * @brief 启动服务器
         * @param server_name 服务器名称
         * @param callback 接收数据的回调函数
         * @return 成功返回true，失败返回错误信息
         */
        virtual BoolResult Start(const std::string& server_name, ServerRecvDataCallback callback) = 0;

        /**
         * @brief 停止服务器
         */
        virtual void Stop() = 0;

        /**
         * @brief 向客户端发送数据
         * @param id 客户端标识
         * @param data 待发送的数据
         * @return 成功返回true，失败返回错误信息
         */
        virtual BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data) = 0;
    };

    /**
     * @brief 创建客户端连接实现
     * @return 客户端连接实现指针
     */
    std::unique_ptr<IClientConnectionImpl> CreateClientConnectionImpl();

    /**
     * @brief 创建服务器实现
     * @return 服务器实现指针
     */
    std::unique_ptr<IServerImpl> CreateServerImpl();
} // namespace tyke

#endif // IPC_INTERNAL_PLATFORM_H_
