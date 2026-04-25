/**
 * @file ipc_client.h
 * @brief IPC客户端声明。提供同步/异步发送接口，支持ECDH+AES-GCM加密通信。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <chrono>
#include <memory>
#include <string_view>

#include "common/tyke_def.h"
#include "ipc_def.h"

namespace tyke
{
class IpcConnection
{
public:
    IpcConnection();


    ~IpcConnection();


    [[nodiscard]] BoolResult Connect(std::string_view server_name, uint32_t timeout_ms = kIpcDefaultTimeoutMs,
                                     uint32_t rw_timeout_ms = kIpcDefaultTimeoutMs);

    [[nodiscard]] BoolResult WriteEncrypted(const void *data, size_t size, uint32_t timeout_ms = kIpcDefaultTimeoutMs);

    [[nodiscard]] BoolResult ReadLoop(const ClientRecvDataCallback &callback,
                                      uint32_t                      timeout_ms = kIpcDefaultTimeoutMs);

    void Close();


    [[nodiscard]] bool IsValid() const;

private:
    std::unique_ptr<class IClientConnectionImpl> impl_;
};


class IpcClient
{
public:
    IpcClient() = delete;


    static BoolResult Send(std::string_view server_name, const std::vector<uint8_t> &request,
                           const ClientRecvDataCallback &callback, uint32_t timeout_ms = kIpcDefaultTimeoutMs);


    static BoolResult SendAsync(std::string_view server_name, const std::vector<uint8_t> &request,
                                uint32_t timeout_ms = kIpcDefaultTimeoutMs);
};
}// namespace tyke