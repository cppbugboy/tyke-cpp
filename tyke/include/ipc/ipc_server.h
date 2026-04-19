/**
 * @file ipc_server.h
 * @brief IPC服务端声明。跨平台IPC服务器，支持Windows命名管道和Linux域套接字。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include "ipc_types.h"
#include "common/tyke_result.h"
#include <memory>
#include <string>
#include <string_view>

namespace tyke
{

    class IpcServer
    {
    public:

        IpcServer();


        ~IpcServer();

        IpcServer(const IpcServer&) = delete;
        IpcServer& operator=(const IpcServer&) = delete;


        BoolResult Start(std::string_view server_name, ServerRecvDataCallback callback) const;


        void Stop() const;


        BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data) const;

    private:
        std::unique_ptr<class IServerImpl> impl_;
    };
}
