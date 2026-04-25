/**
 * @file ipc_server.h
 * @brief IPC服务端声明。跨平台IPC服务器，支持Windows命名管道和Linux域套接字。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "common/tyke_def.h"
#include "ipc_def.h"

namespace tyke
{
    class IpcServer
    {
    public:
        IpcServer();

        ~IpcServer();

        [[nodiscard]] BoolResult Start(std::string_view server_name, ServerRecvDataCallback callback) const;


        void Stop() const;


        [[nodiscard]] BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data) const;

    private:
        std::unique_ptr<class IServerImpl> impl_;
    };

    inline IpcServer& GetGlobalIpcServer()
    {
        static IpcServer instance;
        return instance;
    }
} // namespace tyke