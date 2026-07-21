/**
 * @file ipc_internal_platform.h
 * @brief IPC平台内部抽象接口。声明平台无关的连接实现类和服务端实现类的纯虚接口，
 *        通过工厂函数创建平台相关实例（Windows命名管道 / Linux Unix域套接字）。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 本文件定义了IPC模块的平台抽象层（PIMPL模式）：
 * - IClientConnectionImpl: 客户端连接实现的纯虚接口
 * - IServerImpl: 服务端实现的纯虚接口
 * - 工厂函数根据编译平台返回对应的实现实例
 *
 * @note IServerImpl的工厂返回shared_ptr以支持实现类通过shared_from_this
 *       在异步IOCP/epoll回调中安全延长生命周期，避免Shutdown后use-after-free。
 */

#pragma once

#include <memory>
#include <string_view>

#include "tyke/common/tyke_def.h"
#include "ipc_def.h"

namespace tyke
{
    /**
     * @brief 客户端连接实现接口（PIMPL模式）
     *
     * 定义平台无关的客户端连接操作，由平台相关子类实现。
     * Windows下基于命名管道，Linux下基于Unix域套接字。
     *
     * @note 所有方法均为纯虚函数，调用者不感知平台差异。
     */
    class IClientConnectionImpl
    {
    public:
        virtual ~IClientConnectionImpl() = default;

        /**
         * @brief 连接到指定名称的IPC服务端
         * @param server_name 服务名称
         * @param timeout_ms 连接超时（毫秒）
         * @param rw_timeout_ms 读写超时（毫秒）
         * @return BoolResult 成功返回true，失败返回错误信息
         */
        [[nodiscard]] virtual BoolResult Connect(std::string_view server_name, uint32_t timeout_ms, uint32_t rw_timeout_ms) = 0;

        /**
         * @brief 发送明文数据到服务端
         * @param data 数据指针
         * @param size 数据大小（字节）
         * @param timeout_ms 发送超时（毫秒）
         * @return BoolResult 成功返回true
         * @note 大于64KB的数据会自动分片发送
         */
        [[nodiscard]] virtual BoolResult Write(const void* data, size_t size, uint32_t timeout_ms) = 0;

        /**
         * @brief 启动阻塞式读取循环
         * @param callback 数据接收回调，返回true停止读取
         * @param timeout_ms 单次读取超时（毫秒）
         * @return BoolResult 成功返回true
         * @note 分片消息自动重组后调用回调
         */
        [[nodiscard]] virtual BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms) = 0;

        /**
         * @brief 关闭连接并释放平台资源
         */
        virtual void Close() = 0;

        /**
         * @brief 查询连接是否有效
         * @return true 连接已建立且可通信；false 未连接或已断开
         */
        [[nodiscard]] virtual bool IsValid() const = 0;
    };

    /**
     * @brief 服务端实现接口（PIMPL模式）
     *
     * 定义平台无关的服务端操作，由平台相关子类实现。
     * Windows下使用IOCP + 命名管道，Linux下使用epoll + Unix域套接字。
     */
    class IServerImpl
    {
    public:
        virtual ~IServerImpl() = default;

        /**
         * @brief 启动IPC服务，开始监听客户端连接
         * @param server_name 服务名称，客户端通过此名称连接
         * @param callback 数据接收回调，在工作线程中调用
         * @return BoolResult 成功返回true
         */
        [[nodiscard]] virtual BoolResult Start(std::string_view server_name, ServerRecvDataCallback callback) = 0;

        /**
         * @brief 停止服务，关闭所有客户端连接并释放资源
         */
        virtual void Stop() = 0;

        /**
         * @brief 向指定客户端发送明文数据
         * @param id 客户端标识（由回调提供）
         * @param data 要发送的数据
         * @return BoolResult 成功返回true
         * @note 大于64KB的数据自动分片发送
         */
        [[nodiscard]] virtual BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data) = 0;
    };

    /**
     * @brief 创建平台相关的客户端连接实现
     * @return std::unique_ptr<IClientConnectionImpl> 平台相关实现实例
     */
    std::unique_ptr<IClientConnectionImpl> CreateClientConnectionImpl();

    /**
     * @brief 创建平台相关的服务端实现
     * @return std::shared_ptr<IServerImpl> 平台相关实现实例
     * @note 返回shared_ptr以支持实现类在异步IOCP/epoll回调中
     *       通过shared_from_this安全延长生命周期，避免Shutdown后
     *       回调访问已释放对象（use-after-free）。
     */
    std::shared_ptr<IServerImpl> CreateServerImpl();
} // namespace tyke
