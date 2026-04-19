/**
 * @file ipc_client.h
 * @brief IPC客户端声明。提供同步/异步发送接口，支持ECDH+AES-GCM加密通信。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef IPC_CLIENT_H_
#define IPC_CLIENT_H_

#include "ipc_types.h"
#include "common/tyke_result.h"
#include <memory>
#include <chrono>

namespace tyke
{

    class IpcConnection
    {
    public:

        IpcConnection();


        ~IpcConnection();


        BoolResult Connect(const std::string& server_name, uint32_t timeout_ms = kIpcDefaultTimeoutMs,
                     uint32_t rw_timeout_ms = kIpcDefaultTimeoutMs);


        BoolResult WriteEncrypted(const void* data, size_t size, uint32_t timeout_ms = kIpcDefaultTimeoutMs);


        BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms = kIpcDefaultTimeoutMs);


        void Close();


        bool IsValid() const;


        void UpdateLastUsedTime() { last_used_ = std::chrono::steady_clock::now(); }


        std::chrono::steady_clock::time_point GetLastUsedTime() const { return last_used_; }

    private:
        std::unique_ptr<class IClientConnectionImpl> impl_;
        std::chrono::steady_clock::time_point last_used_;
    };


    class IpcClient
    {
    public:
        IpcClient() = delete;


        static BoolResult Send(const std::string& server_name, const std::vector<uint8_t>& request,
                         const ClientRecvDataCallback& callback, uint32_t timeout_ms = kIpcDefaultTimeoutMs);


        static BoolResult SendAsync(const std::string& server_name, const std::vector<uint8_t>& request,
                              uint32_t timeout_ms = kIpcDefaultTimeoutMs);
    };
}

#endif
