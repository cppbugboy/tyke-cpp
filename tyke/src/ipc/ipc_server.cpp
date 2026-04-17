/**
 * @file ipc_server.cpp
 * @brief IPC服务器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现IpcServer类的具体逻辑，封装平台相关的服务器实现。
 */

#include "ipc/ipc_server.h"
#include "ipc/ipc_internal_platform.h"
#include "common/log_def.h"

namespace tyke
{
    /**
     * @brief 构造函数
     */
    IpcServer::IpcServer() : impl_(CreateServerImpl())
    {
        LOG_DEBUG("IpcServer constructed");
    }

    /**
     * @brief 析构函数
     */
    IpcServer::~IpcServer()
    {
        Stop();
        LOG_DEBUG("IpcServer destructed");
    }

    /**
     * @brief 启动IPC服务器
     * @param server_name 服务器名称
     * @param callback 接收数据的回调函数
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult IpcServer::Start(const std::string& server_name, ServerRecvDataCallback callback)
    {
        LOG_INFO("IpcServer starting, server_name={}", server_name);
        auto result = impl_->Start(server_name, std::move(callback));
        if (!result)
        {
            LOG_ERROR("IpcServer start failed: {}", result.error());
            return nonstd::make_unexpected("server start failed: " + result.error());
        }
        LOG_INFO("IpcServer started successfully");
        return true;
    }

    /**
     * @brief 停止IPC服务器
     */
    void IpcServer::Stop()
    {
        LOG_INFO("IpcServer stopping");
        impl_->Stop();
        LOG_INFO("IpcServer stopped");
    }

    /**
     * @brief 向指定客户端发送数据
     * @param id 客户端标识
     * @param data 待发送的数据
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult IpcServer::SendToClient(ClientId id, const std::vector<uint8_t>& data)
    {
        LOG_DEBUG("SendToClient: client_id={}, data_size={}", id, data.size());
        auto result = impl_->SendToClient(id, data);
        if (!result)
        {
            LOG_ERROR("SendToClient failed: client_id={}, error={}", id, result.error());
            return nonstd::make_unexpected("send to client failed: " + result.error());
        }
        return true;
    }
} // namespace tyke
