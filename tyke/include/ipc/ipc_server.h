/**
 * @file ipc_server.h
 * @brief IPC服务端声明。跨平台IPC服务器，支持Windows命名管道和Linux域套接字。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef IPC_SERVER_H_
#define IPC_SERVER_H_

#include "ipc_types.h"
#include "common/tyke_result.h"
#include <memory>
#include <string>

namespace tyke
{

    class IpcServer
    {
    public:

        IpcServer();


        ~IpcServer();

        IpcServer(const IpcServer&) = delete;
        IpcServer& operator=(const IpcServer&) = delete;


        BoolResult Start(const std::string& server_name, ServerRecvDataCallback callback);


        void Stop();


        BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data);

    private:
        std::unique_ptr<class IServerImpl> impl_;
    };
}

#endif
