/**
 * @file ipc_client.cpp
 * @brief IPC 客户端实现。IpcConnection 封装单连接生命周期，IpcClient 提供 Send/SendAsync 静态方法。
 *
 * Send 为同步请求-响应模式（获取连接 -> 写入 -> 读循环 -> 归还/销毁连接）。
 * SendAsync 为即发即弃模式（获取连接 -> 写入 -> 立即归还连接以复用）。
 *
 * @author Nick
 * @date 2026/04/19
 */

#include "tyke/ipc/ipc_client.h"

#include "tyke/common/log_def.h"
#include "tyke/ipc/connection_pool_factory.h"
#include "tyke/ipc/ipc_internal_platform.h"

namespace tyke
{
    /** @brief 构造连接，创建平台实现对象。 */
    IpcConnection::IpcConnection() : impl_(CreateClientConnectionImpl())
    {
        LOG_DEBUG("IpcConnection constructed");
    }

    /** @brief 析构时关闭连接。 */
    IpcConnection::~IpcConnection()
    {
        Close();
        LOG_DEBUG("IpcConnection destructed");
    }

    /**
     * @brief 连接到指定服务端。
     *
     * @param server_name 服务端名称
     * @param timeout_ms 连接超时（毫秒）
     * @param rw_timeout_ms 读写超时（毫秒）
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult IpcConnection::Connect(std::string_view server_name, uint32_t timeout_ms, const uint32_t rw_timeout_ms)
    {
        LOG_DEBUG("Connecting to server: server_name={}, timeout={}ms", server_name, timeout_ms);
        if (auto result = impl_->Connect(server_name, timeout_ms, rw_timeout_ms); !result)
        {
            LOG_ERROR("Connect failed: {}", result.error());
            return nonstd::make_unexpected("connect failed: " + result.error());
        }
        LOG_DEBUG("Connected to server: {}", server_name);
        return true;
    }

    /**
     * @brief 向连接写入数据。
     *
     * 内部处理大消息的分片（> kFragmentChunkSize 时自动分片）。
     *
     * @param data 数据指针
     * @param size 数据大小
     * @param timeout_ms 超时（毫秒）
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult IpcConnection::Write(const void* data, size_t size, uint32_t timeout_ms)
    {
        LOG_DEBUG("Write: size={}, timeout={}ms", size, timeout_ms);
        if (auto result = impl_->Write(data, size, timeout_ms); !result)
        {
            LOG_ERROR("Write failed: {}", result.error());
            return nonstd::make_unexpected("write failed: " + result.error());
        }
        return true;
    }

    /**
     * @brief 进入读循环，持续读取并调用回调直到回调返回 true 或连接关闭。
     *
     * @param callback 接收数据的回调，返回 true 表示完成并退出循环
     * @param timeout_ms 读写超时（毫秒）
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult IpcConnection::ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms)
    {
        LOG_DEBUG("ReadLoop: timeout={}ms", timeout_ms);
        if (auto result = impl_->ReadLoop(callback, timeout_ms); !result)
        {
            LOG_ERROR("ReadLoop failed: {}", result.error());
            return nonstd::make_unexpected("read loop failed: " + result.error());
        }
        return true;
    }

    /** @brief 关闭连接并释放平台资源。 */
    void IpcConnection::Close()
    {
        LOG_DEBUG("Closing connection");
        impl_->Close();
    }

    /** @brief 检查连接是否有效（管道/套接字句柄是否有效）。 */
    bool IpcConnection::IsValid() const
    {
        return impl_->IsValid();
    }

    /**
     * @brief 同步发送请求并等待响应。
     *
     * 从连接池获取连接 -> 写入请求 -> 读循环等待响应（回调解析） -> 释放连接。
     * 若写入或读取失败，连接标记为需重连（Release with should_reconnect=true）。
     *
     * @param server_name 目标服务端名称
     * @param request 请求数据
     * @param callback 接收响应数据的回调（返回 true 表示完成）
     * @param timeout_ms 超时（毫秒）
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult IpcClient::Send(std::string_view server_name, const std::vector<uint8_t>& request,
                               const ClientRecvDataCallback& callback, const uint32_t timeout_ms)
    {
        LOG_DEBUG("IpcClient::Send: server_name={}, request_size={} bytes", server_name, request.size());

        const auto pool = GetGlobalConnectionPoolFactory().GetPool(std::string(server_name));
        auto conn_result = pool->Acquire();
        if (!conn_result)
        {
            LOG_ERROR("IpcClient::Send acquire connection failed: {}", conn_result.error());
            return nonstd::make_unexpected("send: " + conn_result.error());
        }

        auto& conn = *conn_result.value();

        if (auto write_result = conn.Write(request.data(), request.size(), timeout_ms); !write_result)
        {
            LOG_ERROR("IpcClient::Send write failed: {}", write_result.error());
            conn_result.value().release();
            pool->Release(&conn, true);
            return nonstd::make_unexpected("send: " + write_result.error());
        }

        if (auto read_result = conn.ReadLoop(callback, timeout_ms); !read_result)
        {
            LOG_ERROR("IpcClient::Send read failed: {}", read_result.error());
            conn_result.value().release();
            pool->Release(&conn, true);
            return nonstd::make_unexpected("send: " + read_result.error());
        }

        LOG_DEBUG("IpcClient::Send completed successfully");
        return true;
    }

    /**
     * @brief 异步即发即弃发送请求。
     *
     * 从连接池获取连接 -> 写入请求 -> 立即归还连接以复用。
     * 异步响应由高层 stub/ResponseRouter 机制处理。
     *
     * @note 与 Go 侧 IPCClientSendAsync 行为对齐：写入后不再等待响应。
     * @param server_name 目标服务端名称
     * @param request 请求数据
     * @param timeout_ms 写入超时（毫秒）
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult IpcClient::SendAsync(std::string_view server_name, const std::vector<uint8_t>& request,
                                    const uint32_t timeout_ms)
    {
        LOG_DEBUG("IpcClient::SendAsync: server_name={}, request_size={} bytes", server_name, request.size());

        const auto pool = GetGlobalConnectionPoolFactory().GetPool(std::string(server_name));
        auto conn_result = pool->Acquire();
        if (!conn_result)
        {
            LOG_ERROR("IpcClient::SendAsync acquire connection failed: {}", conn_result.error());
            return nonstd::make_unexpected("send async: " + conn_result.error());
        }

        auto& conn = *conn_result.value();

        if (auto write_result = conn.Write(request.data(), request.size(), timeout_ms); !write_result)
        {
            LOG_ERROR("IpcClient::SendAsync write failed: {}", write_result.error());
            conn_result.value().release();
            pool->Release(&conn, true);
            return nonstd::make_unexpected("send async: write failed");
        }

        // 成功路径显式归还连接以复用。
        // SendAsync 语义为"即发即弃"，异步响应由高层 stub/ResponseRouter 机制处理，
        // 连接可安全复用。
        conn_result.value().release();
        pool->Release(&conn, false);

        LOG_DEBUG("IpcClient::SendAsync completed successfully");
        return true;
    }
} // namespace tyke
