/**
 * @file data_handler.h
 * @brief 数据处理器声明。解析IPC数据包并通过MessageDispatcher分发到相应处理器。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include "tyke_request.h"
#include "ipc/ipc_server.h"

namespace tyke::data_handler
{
    std::optional<uint32_t> DataCallback(ClientId client_id, const std::vector<unsigned char>& data_vec,
                                 const SendDataHandler& send_data_handler);

    
    void RequestHandler(ClientId client_id, const TykeRequest& request,
                               const SendDataHandler& send_data_handler);

    
    void RequestHandlerAsync(const TykeRequest& request);

    
    void ResponseHandler(const TykeResponse& response);
}
