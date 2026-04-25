/**
 * @file ipc_internal_platform.h
 * @brief IPC平台内部定义。声明平台相关的连接上下文和服务端实现类。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <memory>
#include <string_view>

#include "ipc_def.h"
#include "common/tyke_def.h"

namespace tyke
{
    class IClientConnectionImpl
    {
    public:
        virtual ~IClientConnectionImpl() = default;


        virtual BoolResult Connect(std::string_view server_name, uint32_t timeout_ms, uint32_t rw_timeout_ms) = 0;


        virtual BoolResult WriteEncrypted(const void* data, size_t size, uint32_t timeout_ms) = 0;


        virtual BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms) = 0;


        virtual void Close() = 0;


        virtual bool IsValid() const = 0;
    };


    class IServerImpl
    {
    public:
        virtual ~IServerImpl() = default;


        virtual BoolResult Start(std::string_view server_name, ServerRecvDataCallback callback) = 0;


        virtual void Stop() = 0;


        virtual BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data) = 0;
    };


    std::unique_ptr<IClientConnectionImpl> CreateClientConnectionImpl();


    std::unique_ptr<IServerImpl> CreateServerImpl();
}