/**
 * @file ipc_client.cpp
 * @brief IPC客户端实现。提供同步/异步发送接口，支持ECDH+AES-GCM加密通信。
 * @author Nick
 * @date 2026/04/19
 */

#include "ipc/ipc_client.h"

#include "ipc/ipc_internal_platform.h"
#include "ipc/connection_pool_factory.h"
#include "common/log_def.h"

namespace tyke
{
    IpcConnection::IpcConnection() : impl_(CreateClientConnectionImpl())
    {
        LOG_DEBUG("IpcConnection constructed");
    }

    IpcConnection::~IpcConnection()
    {
        Close();
        LOG_DEBUG("IpcConnection destructed");
    }

    BoolResult IpcConnection::Connect(std::string_view server_name, uint32_t timeout_ms,
                                      const uint32_t rw_timeout_ms) const
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

    BoolResult IpcConnection::WriteEncrypted(const void* data, size_t size, uint32_t timeout_ms) const
    {
        LOG_DEBUG("WriteEncrypted: size={}, timeout={}ms", size, timeout_ms);
        if (auto result = impl_->WriteEncrypted(data, size, timeout_ms); !result)
        {
            LOG_ERROR("WriteEncrypted failed: {}", result.error());
            return nonstd::make_unexpected("write encrypted failed: " + result.error());
        }
        return true;
    }

    BoolResult IpcConnection::ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms) const
    {
        LOG_DEBUG("ReadLoop: timeout={}ms", timeout_ms);
        if (auto result = impl_->ReadLoop(callback, timeout_ms); !result)
        {
            LOG_ERROR("ReadLoop failed: {}", result.error());
            return nonstd::make_unexpected("read loop failed: " + result.error());
        }
        return true;
    }

    void IpcConnection::Close() const
    {
        LOG_DEBUG("Closing connection");
        impl_->Close();
    }

    bool IpcConnection::IsValid() const
    {
        return impl_->IsValid();
    }

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

        IpcConnection* conn = conn_result.value();
        bool should_reconnect = false;

        if (auto write_result = conn->WriteEncrypted(request.data(), request.size(), timeout_ms); !write_result)
        {
            LOG_ERROR("IpcClient::Send write failed: {}", write_result.error());
            should_reconnect = true;
            pool->Release(conn, should_reconnect);
            return nonstd::make_unexpected("send: " + write_result.error());
        }

        if (auto read_result = conn->ReadLoop(callback, timeout_ms); !read_result)
        {
            LOG_ERROR("IpcClient::Send read failed: {}", read_result.error());
            should_reconnect = true;
            pool->Release(conn, should_reconnect);
            return nonstd::make_unexpected("send: " + read_result.error());
        }

        pool->Release(conn, should_reconnect);
        LOG_DEBUG("IpcClient::Send completed successfully");
        return true;
    }

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

        IpcConnection* conn = conn_result.value();
        bool should_reconnect = false;

        if (auto write_result = conn->WriteEncrypted(request.data(), request.size(), timeout_ms); !write_result)
        {
            LOG_ERROR("IpcClient::SendAsync write failed: {}", write_result.error());
            should_reconnect = true;
        }

        pool->Release(conn, should_reconnect);

        if (should_reconnect)
        {
            return nonstd::make_unexpected("send async: write encrypted failed");
        }

        LOG_DEBUG("IpcClient::SendAsync completed successfully");
        return true;
    }
}
