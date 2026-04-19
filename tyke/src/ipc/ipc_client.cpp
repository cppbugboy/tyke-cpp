/**
 * @file ipc_client.cpp
 * @brief IPC客户端实现。提供同步/异步发送接口，支持ECDH+AES-GCM加密通信。
 * @author Nick
 * @date 2026/04/19
 */

#include "ipc/ipc_client.h"
#include "ipc/ipc_internal_platform.h"
#include "common/log_def.h"

namespace tyke
{
    IpcConnection::IpcConnection() : impl_(CreateClientConnectionImpl())
    {
        UpdateLastUsedTime();
        LOG_DEBUG("IpcConnection constructed");
    }
    IpcConnection::~IpcConnection()
    {
        Close();
        LOG_DEBUG("IpcConnection destructed");
    }
    BoolResult IpcConnection::Connect(const std::string& server_name, uint32_t timeout_ms, uint32_t rw_timeout_ms)
    {
        LOG_DEBUG("Connecting to server: server_name={}, timeout={}ms", server_name, timeout_ms);
        auto result = impl_->Connect(server_name, timeout_ms, rw_timeout_ms);
        if (!result)
        {
            LOG_ERROR("Connect failed: {}", result.error());
            return nonstd::make_unexpected("connect failed: " + result.error());
        }
        LOG_DEBUG("Connected to server: {}", server_name);
        return true;
    }
    BoolResult IpcConnection::WriteEncrypted(const void* data, size_t size, uint32_t timeout_ms)
    {
        LOG_DEBUG("WriteEncrypted: size={}, timeout={}ms", size, timeout_ms);
        auto result = impl_->WriteEncrypted(data, size, timeout_ms);
        if (!result)
        {
            LOG_ERROR("WriteEncrypted failed: {}", result.error());
            return nonstd::make_unexpected("write encrypted failed: " + result.error());
        }
        return true;
    }
    BoolResult IpcConnection::ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms)
    {
        LOG_DEBUG("ReadLoop: timeout={}ms", timeout_ms);
        auto result = impl_->ReadLoop(callback, timeout_ms);
        if (!result)
        {
            LOG_ERROR("ReadLoop failed: {}", result.error());
            return nonstd::make_unexpected("read loop failed: " + result.error());
        }
        return true;
    }
    void IpcConnection::Close()
    {
        LOG_DEBUG("Closing connection");
        impl_->Close();
    }
    bool IpcConnection::IsValid() const
    {
        return impl_->IsValid();
    }
    BoolResult IpcClient::Send(const std::string& server_name, const std::vector<uint8_t>& request,
                               const ClientRecvDataCallback& callback, uint32_t timeout_ms)
    {
        LOG_DEBUG("IpcClient::Send: server_name={}, request_size={} bytes", server_name, request.size());

        IpcConnection conn;
        auto connect_result = conn.Connect(server_name, timeout_ms, timeout_ms);
        if (!connect_result)
        {
            LOG_ERROR("IpcClient::Send connect failed: {}", connect_result.error());
            return nonstd::make_unexpected("send: " + connect_result.error());
        }

        auto write_result = conn.WriteEncrypted(request.data(), request.size(), timeout_ms);
        if (!write_result)
        {
            LOG_ERROR("IpcClient::Send write failed: {}", write_result.error());
            return nonstd::make_unexpected("send: " + write_result.error());
        }

        auto read_result = conn.ReadLoop(callback, timeout_ms);
        if (!read_result)
        {
            LOG_ERROR("IpcClient::Send read failed: {}", read_result.error());
            return nonstd::make_unexpected("send: " + read_result.error());
        }

        LOG_DEBUG("IpcClient::Send completed successfully");
        return true;
    }
    BoolResult IpcClient::SendAsync(const std::string& server_name, const std::vector<uint8_t>& request,
                                    uint32_t timeout_ms)
    {
        LOG_DEBUG("IpcClient::SendAsync: server_name={}, request_size={} bytes", server_name, request.size());

        IpcConnection conn;
        auto connect_result = conn.Connect(server_name, timeout_ms, timeout_ms);
        if (!connect_result)
        {
            LOG_ERROR("IpcClient::SendAsync connect failed: {}", connect_result.error());
            return nonstd::make_unexpected("send async: " + connect_result.error());
        }

        auto write_result = conn.WriteEncrypted(request.data(), request.size(), timeout_ms);
        if (!write_result)
        {
            LOG_ERROR("IpcClient::SendAsync write failed: {}", write_result.error());
            return nonstd::make_unexpected("send async: " + write_result.error());
        }

        LOG_DEBUG("IpcClient::SendAsync completed successfully");
        return true;
    }
}
