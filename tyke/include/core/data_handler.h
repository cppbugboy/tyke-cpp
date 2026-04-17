/**
 * @file data_handler.h
 * @brief 数据处理器
 * @author Nick
 * @date 2026/04/17
 *
 * DataHandler负责处理IPC层接收到的原始数据，根据消息类型进行分发处理。
 * 是IPC层与业务逻辑层之间的桥梁。
 */

#pragma once

#include "tyke_request.h"
#include "ipc/ipc_server.h"


namespace tyke
{
    /**
     * @brief 数据处理器类
     *
     * 提供静态方法处理IPC层接收的数据，根据消息类型分发到相应的处理流程。
     * 支持同步请求、异步请求和响应三种消息类型的处理。
     */
    class DataHandler
    {
    public:
        DataHandler() = delete;
        ~DataHandler() = delete;

        /**
         * @brief IPC数据回调函数
         * @param client_id 客户端标识
         * @param data_vec 接收到的原始数据
         * @param send_data_handler 发送数据的回调函数
         * @return 已处理的数据字节数
         *
         * 作为IPC服务器的数据接收回调，解析数据包并分发到相应的处理器。
         */
        static uint32_t DataCallback(ClientId client_id, const std::vector<unsigned char>& data_vec,
                                     const SendDataHandler& send_data_handler);

        /**
         * @brief 处理同步请求
         * @param client_id 客户端标识
         * @param request 请求对象
         * @param send_data_handler 发送数据的回调函数
         *
         * 处理同步请求，分发到对应的路由处理器，并返回响应。
         */
        static void RequestHandler(ClientId client_id, const TykeRequest& request,
                                   const SendDataHandler& send_data_handler);

        /**
         * @brief 处理异步请求
         * @param request 请求对象
         *
         * 处理异步请求，分发到对应的路由处理器，响应通过异步方式返回。
         */
        static void RequestHandlerAsync(const TykeRequest& request);

        /**
         * @brief 处理响应
         * @param response 响应对象
         *
         * 根据响应类型分发到对应的处理器（回调函数或Future）。
         */
        static void ResponseHandler(const TykeResponse& response);
    };
} // namespace tyke