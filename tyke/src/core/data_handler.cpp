/**
 * @file data_handler.cpp
 * @brief 数据处理器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现DataHandler类的具体逻辑，处理IPC层接收的数据并分发到相应的处理器。
 */

#include "core/data_handler.h"

#include "common/log_def.h"
#include "core/data_proc.h"
#include "core/dispatcher.h"
#include "core/request_stub.h"
#include "core/tyke_request.h"
#include "core/tyke_response.h"

namespace tyke
{
    /**
     * @brief IPC数据回调函数
     * @param client_id 客户端标识
     * @param data_vec 接收到的原始数据
     * @param send_data_handler 发送数据的回调函数
     * @return 已处理的数据字节数
     */
    uint32_t DataHandler::DataCallback(const ClientId client_id, const std::vector<unsigned char>& data_vec,
                                       const SendDataHandler& send_data_handler)
    {
        LOG_DEBUG("DataCallback invoked, client_id={}, data_size={}", client_id, data_vec.size());

        // 检查数据长度是否足够包含协议头
        if (data_vec.size() <= sizeof(ProtocolHeader))
        {
            LOG_WARN("Data too short for protocol header, size={}", data_vec.size());
            return 0;
        }

        // 解析协议头
        ProtocolHeader header;
        constexpr size_t header_size = sizeof(ProtocolHeader);
        std::memcpy(&header, data_vec.data(), header_size);

        // 验证协议魔数
        if (std::memcmp(header.magic, kProtocolMagic, sizeof(header.magic)) != 0)
        {
            LOG_WARN("Protocol magic mismatch, expected=TYKE");
            return 0;
        }

        LOG_DEBUG("Received message, type={}, metadata_len={}, content_len={}",
                  static_cast<int>(header.msg_type), header.metadata_len, header.content_len);

        uint32_t used = 0;
        switch (header.msg_type)
        {
        case MessageType::kRequest:
            {
                // 处理同步请求
                const auto tyke_request_ptr = MakeRequestPtr();
                auto decode_result = DataProc::DecodeRequest(data_vec, *tyke_request_ptr, used);
                if (decode_result)
                {
                    LOG_DEBUG("Processing sync request, route={}", tyke_request_ptr->GetRoute());
                    RequestHandler(client_id, *tyke_request_ptr, send_data_handler);
                }
                else
                {
                    LOG_ERROR("Decode request failed: {}", decode_result.error());
                }
                break;
            }
        case MessageType::kRequestAsync:
        case MessageType::kRequestAsyncFunc:
        case MessageType::kRequestAsyncFuture:
            {
                // 处理异步请求
                const auto tyke_request_ptr = MakeRequestPtr();
                auto decode_result = DataProc::DecodeRequest(data_vec, *tyke_request_ptr, used);
                if (decode_result)
                {
                    LOG_DEBUG("Processing async request, route={}, msg_type={}",
                              tyke_request_ptr->GetRoute(), static_cast<int>(header.msg_type));
                    RequestHandlerAsync(*tyke_request_ptr);
                }
                else
                {
                    LOG_ERROR("Decode async request failed: {}", decode_result.error());
                }
                break;
            }
        case MessageType::kResponseAsync:
        case MessageType::kResponseAsyncFunc:
        case MessageType::kResponseAsyncFuture:
            {
                // 处理异步响应
                const auto tyke_response_ptr = MakeResponsePtr();
                auto decode_result = DataProc::DecodeResponse(data_vec, *tyke_response_ptr, used);
                if (decode_result)
                {
                    LOG_DEBUG("Processing async response, route={}, msg_uuid={}",
                              tyke_response_ptr->GetRoute(), tyke_response_ptr->GetMsgUuid());
                    ResponseHandler(*tyke_response_ptr);
                }
                else
                {
                    LOG_ERROR("Decode response failed: {}", decode_result.error());
                }
                break;
            }
        default:
            LOG_WARN("Unknown message type: {}", static_cast<int>(header.msg_type));
            break;
        }
        return used;
    }

    /**
     * @brief 处理同步请求
     * @param client_id 客户端标识
     * @param request 请求对象
     * @param send_data_handler 发送数据的回调函数
     */
    void DataHandler::RequestHandler(const ClientId client_id, const TykeRequest& request,
                                     const SendDataHandler& send_data_handler)
    {
        LOG_DEBUG("RequestHandler: client_id={}, route={}, msg_uuid={}",
                  client_id, request.GetRoute(), request.GetMsgUuid());

        auto response_ptr = MakeResponsePtr();
        response_ptr->SetIpcFD(client_id)
                .SetMessageType(MessageType::kResponse)
                .SetModule(request.GetModule())
                .SetMsgUuid(request.GetMsgUuid())
                .SetRoute(request.GetRoute())
                .SetAsyncUuid(request.GetAsyncUuid())
                .SetSendDataHandler(send_data_handler);

        // 分发请求到处理器
        Dispatcher::DispatchRequest(request, *response_ptr);

        // 发送响应
        auto send_result = response_ptr->Send();
        if (!send_result)
        {
            LOG_ERROR("Send response failed: {}", send_result.error());
        }
    }

    /**
     * @brief 处理异步请求
     * @param request 请求对象
     */
    void DataHandler::RequestHandlerAsync(const TykeRequest& request)
    {
        LOG_DEBUG("RequestHandlerAsync: route={}, msg_uuid={}",
                  request.GetRoute(), request.GetMsgUuid());

        auto response_ptr = MakeResponsePtr();
        response_ptr->SetAsyncUuid(request.GetAsyncUuid())
                .SetMessageType(MessageType::kResponseAsync)
                .SetModule(request.GetModule())
                .SetMsgUuid(request.GetMsgUuid())
                .SetRoute(request.GetRoute());

        // 根据请求类型设置响应类型
        switch (request.GetMessageType())
        {
        case MessageType::kRequestAsync: response_ptr->SetMessageType(MessageType::kResponseAsync); break;
        case MessageType::kRequestAsyncFunc: response_ptr->SetMessageType(MessageType::kResponseAsyncFunc); break;
        case MessageType::kRequestAsyncFuture: response_ptr->SetMessageType(MessageType::kResponseAsyncFuture); break;
        default: break;
        }

        // 分发请求到处理器
        Dispatcher::DispatchRequest(request, *response_ptr);

        // 异步发送响应
        auto send_result = response_ptr->SendAsync();
        if (!send_result)
        {
            LOG_ERROR("Send async response failed: {}", send_result.error());
        }
    }

    /**
     * @brief 处理响应
     * @param response 响应对象
     */
    void DataHandler::ResponseHandler(const TykeResponse& response)
    {
        LOG_DEBUG("ResponseHandler: route={}, msg_uuid={}, msg_type={}",
                  response.GetRoute(), response.GetMsgUuid(), static_cast<int>(response.GetMessageType()));

        switch (response.GetMessageType())
        {
        case MessageType::kResponseAsync:
            // 分发到响应处理器
            Dispatcher::DispatchResponse(response);
            break;
        case MessageType::kResponseAsyncFunc:
            // 执行回调函数
            RequestStub::ExecFunc(response);
            break;
        case MessageType::kResponseAsyncFuture:
            // 设置Future结果
            RequestStub::SetFuture(response);
            break;
        default:
            LOG_WARN("Unknown response type: {}", static_cast<int>(response.GetMessageType()));
            break;
        }
    }
} // tyke
