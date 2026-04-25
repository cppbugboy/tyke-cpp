/**
 * @file data_handler.h
 * @brief 数据处理器声明。解析IPC数据包并通过路由分发到相应处理器。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include "request.h"

namespace tyke::data_handler
{
    std::optional<uint32_t> DataCallback(ClientId client_id, const std::vector<uint8_t>& data_vec,
                                         const SendDataHandler& send_data_handler);


    void RequestHandler(const Request& request, ClientId client_id, const SendDataHandler& send_data_handler);


    void RequestHandlerAsync(const Request& request);


    void ResponseHandler(const Response& response);
}
