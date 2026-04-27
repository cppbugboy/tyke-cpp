/**
 * @file ipc_server.cpp
 * @brief IPC服务端实现。跨平台IPC服务器，支持Windows命名管道和Linux域套接字。
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

    void IpcServer::Stop() const
    {
        LOG_INFO("IpcServer stopping");
        impl_->Stop();
        LOG_INFO("IpcServer stopped");
    }

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