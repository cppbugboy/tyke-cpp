/**
 * @file ipc_server.cpp
 * @brief IPC 服务端实现。跨平台 IPC 服务器外观，委托平台实现处理 Windows 命名管道/Linux 域套接字。
 * @author Nick
 * @date 2026/04/19
 */

#include "tyke/ipc/ipc_server.h"

#include "tyke/common/log_def.h"
#include "tyke/ipc/ipc_internal_platform.h"

namespace tyke
{
    IpcServer::IpcServer() : impl_(CreateServerImpl())
    {
        LOG_DEBUG("IpcServer constructed");
    }

    IpcServer::~IpcServer()
    {
        Stop();
        LOG_DEBUG("IpcServer destructed");
    }

    /**
     * @brief 启动 IPC 服务端，在指定名称上监听客户端连接。
     *
     * @param server_name 服务名称（Windows 对应管道名，Linux 对应抽象域套接字名）
     * @param callback 接收数据回调，签名为 (ClientId, data_vec, send_handler) -> optional<uint32_t>
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult IpcServer::Start(std::string_view server_name, ServerRecvDataCallback callback) const
    {
        LOG_INFO("IpcServer starting, server_name={}", server_name);
        if (auto result = impl_->Start(server_name, std::move(callback)); !result)
        {
            LOG_ERROR("IpcServer start failed: {}", result.error());
            return nonstd::make_unexpected("server start failed: " + result.error());
        }
        LOG_INFO("IpcServer started successfully");
        return true;
    }

    /** @brief 停止 IPC 服务端，关闭所有客户端连接并释放资源。 */
    void IpcServer::Stop() const
    {
        LOG_INFO("IpcServer stopping");
        impl_->Stop();
        LOG_INFO("IpcServer stopped");
    }

    /**
     * @brief 向指定客户端发送数据。
     *
     * @param id 客户端标识
     * @param data 待发送的数据
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult IpcServer::SendToClient(ClientId id, const std::vector<uint8_t>& data) const
    {
        LOG_DEBUG("SendToClient: client_id={}, data_size={}", id, data.size());
        if (auto result = impl_->SendToClient(id, data); !result)
        {
            LOG_ERROR("SendToClient failed: client_id={}, error={}", id, result.error());
            return nonstd::make_unexpected("send to client failed: " + result.error());
        }
        return true;
    }
} // namespace tyke
