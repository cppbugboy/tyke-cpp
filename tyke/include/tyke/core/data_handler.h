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
    /** @brief IPC数据回调入口。解析并解码原始数据包，分流到请求/响应处理。
     * @param client_id 发送方客户端ID。
     * @param data_vec 原始字节流。
     * @param send_data_handler 发送数据的回调函数。
     * @return 成功时返回已处理的字节数，失败时返回 nullopt。
     */
    std::optional<uint32_t> DataCallback(ClientId client_id, const std::vector<uint8_t>& data_vec,
                                         const SendDataHandler& send_data_handler);

    /** @brief 处理同步请求：通过分发器路由到对应的控制器和过滤器链。
     * @param request 已解码的请求对象。
     * @param client_id 请求方客户端ID。
     * @param send_data_handler 用于发送响应的回调函数。
     */
    void RequestHandler(Request& request, ClientId client_id, const SendDataHandler& send_data_handler);

    /** @brief 处理异步请求：通过分发器路由到对应的异步处理器。
     * @param request 已解码的请求对象。
     */
    void RequestHandlerAsync(Request& request);

    /** @brief 处理响应：匹配并触发等待的异步回调或future。
     * @param response 已解码的响应对象。
     */
    void ResponseHandler(Response response);
} // namespace tyke::data_handler
