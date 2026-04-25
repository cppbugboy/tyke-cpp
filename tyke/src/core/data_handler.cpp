/**
 * @file data_handler.cpp
 * @brief 数据处理器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现DataHandler类的具体逻辑，处理IPC层接收的数据并分发到相应的处理器。
 */

#include "core/data_handler.h"

#include <nlohmann/json.hpp>

#include "common/log_def.h"
#include "component/timing_wheel.h"
#include "core/context_factory.h"
#include "core/data_proc.h"
#include "core/dispatcher.h"
#include "core/request.h"
#include "core/request_stub.h"
#include "core/response.h"

namespace tyke::data_handler
{
    std::optional<uint32_t> DataCallback(const ClientId client_id, const std::vector<uint8_t>& data_vec,
                                         const SendDataHandler& send_data_handler)
    {
        try
        {
            LOG_DEBUG("DataCallback invoked, client_id={}, data_size={}", client_id, data_vec.size());

            if (data_vec.size() <= sizeof(ProtocolHeader))
            {
                LOG_WARN("Data too short for protocol header, size={}, discarding", data_vec.size());
                return 0;
            }

            ProtocolHeader header;
            if (!DataProc::PeekHeader(data_vec.data(), data_vec.size(), header))
            {
                LOG_WARN("Failed to peek protocol header, size={}, discarding", data_vec.size());
                return 0;
            }

            if (std::memcmp(header.magic, kProtocolMagic, sizeof(header.magic)) != 0)
            {
                LOG_WARN("Protocol magic mismatch, expected=TYKE, discarding {} bytes", data_vec.size());
                return std::nullopt;
            }

            LOG_DEBUG("Received message, type={}, metadata_len={}, content_len={}", static_cast<int>(header.msg_type),
                      header.metadata_len, header.content_len);

            uint32_t used = 0;
            switch (header.msg_type)
            {
            case MessageType::kRequest:
                {
                    if (const auto tyke_request_ptr = Request::Acquire();
                        DataProc::DecodeRequest(data_vec, *tyke_request_ptr, used))
                    {
                        LOG_DEBUG("Processing sync request, route={}", tyke_request_ptr->GetRoute());
                        RequestHandler(*tyke_request_ptr, client_id, send_data_handler);
                    }
                    else
                    {
                        if (used == 0)
                        {
                            LOG_WARN("Decode request failed, data incomplete, waiting for more data");
                            return std::nullopt;
                        }
                        LOG_WARN("Decode request failed, invalid data, discarding");
                        return 0;
                    }
                    break;
                }
            case MessageType::kRequestAsync:
            case MessageType::kRequestAsyncFunc:
            case MessageType::kRequestAsyncFuture:
                {
                    if (const auto tyke_request_ptr = Request::Acquire();
                        DataProc::DecodeRequest(data_vec, *tyke_request_ptr, used))
                    {
                        LOG_DEBUG("Processing async request, route={}, msg_type={}", tyke_request_ptr->GetRoute(),
                                  static_cast<int>(header.msg_type));
                        RequestHandlerAsync(*tyke_request_ptr);
                    }
                    else
                    {
                        if (used == 0)
                        {
                            LOG_WARN("Decode async request failed, data incomplete, waiting for more data");
                            return std::nullopt;
                        }
                        LOG_WARN("Decode async request failed, invalid data, discarding");
                        return 0;
                    }
                    break;
                }
            case MessageType::kResponseAsync:
            case MessageType::kResponseAsyncFunc:
            case MessageType::kResponseAsyncFuture:
                {
                    if (const auto tyke_response_ptr = Response::Acquire();
                        DataProc::DecodeResponse(data_vec, *tyke_response_ptr, used))
                    {
                        LOG_DEBUG("Processing async response, route={}, msg_uuid={}", tyke_response_ptr->GetRoute(),
                                  tyke_response_ptr->GetMsgUuid());
                        ResponseHandler(*tyke_response_ptr);
                    }
                    else
                    {
                        if (used == 0)
                        {
                            LOG_WARN("Decode async response failed, data incomplete, waiting for more data");
                            return std::nullopt;
                        }
                        LOG_WARN("Decode async response failed, invalid data, discarding");
                        return 0;
                    }
                    break;
                }
            default:
                LOG_WARN("Unknown message type: {}", static_cast<int>(header.msg_type));
                break;
            }
            return used;
        }
        catch (const nlohmann::json::exception& e)
        {
            LOG_ERROR("DataCallback JSON error: id={}, message={}", e.id, e.what());
            return std::nullopt;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("DataCallback exception: {}", e.what());
            return std::nullopt;
        }
        catch (...)
        {
            LOG_ERROR("DataCallback unknown exception");
            return std::nullopt;
        }
    }

    void RequestHandler(const Request& request, const ClientId client_id, const SendDataHandler& send_data_handler)
    {
        LOG_DEBUG("RequestHandler: client_id={}, route={}, msg_uuid={}", client_id, request.GetRoute(),
                  request.GetMsgUuid());

        const auto response_ptr = Response::Acquire();
        response_ptr->SetClientId(client_id)
                    .SetMessageType(MessageType::kResponse)
                    .SetModule(request.GetModule())
                    .SetMsgUuid(request.GetMsgUuid())
                    .SetRoute(request.GetRoute())
                    .SetAsyncUuid(request.GetAsyncUuid())
                    .SetSendDataHandler(send_data_handler);

        const auto [fst, snd] = context::ContextFactory::WithTimeout(context::ContextFactory::Background(),
                                                                     std::chrono::milliseconds(request.GetTimeout()));
        const auto timer_ctx = std::dynamic_pointer_cast<tyke::TimerContext>(fst);
        if (!timer_ctx)
        {
            LOG_ERROR("Failed to cast context to TimerContext");
            response_ptr->SetResult(StatusCode::kInternalError, "internal error");
            if (auto send_result = response_ptr->Send(); !send_result)
            {
                LOG_ERROR("Send response failed: {}", send_result.error());
            }
            return;
        }
        auto token = timer_ctx->RegisterCallback(
            [response_ptr]()
            {
                response_ptr->SetResult(StatusCode::kTimeout, "timeout");
                if (auto send_result = response_ptr->Send(); !send_result)
                {
                    LOG_ERROR("Send response failed: {}", send_result.error());
                }
            });
        timer_ctx->ActivateTimer();

        dispatcher::DispatchRequest(request, *response_ptr, fst);

        if (!response_ptr->IsSent())
        {
            timer_ctx->UnregisterCallback(token);
            if (auto send_result = response_ptr->Send(); !send_result)
            {
                LOG_ERROR("Send response failed: {}", send_result.error());
            }
        }
    }

    void RequestHandlerAsync(const Request& request)
    {
        LOG_DEBUG("RequestHandlerAsync: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());

        const auto response_ptr = Response::Acquire();
        response_ptr->SetAsyncUuid(request.GetAsyncUuid())
                    .SetMessageType(MessageType::kResponseAsync)
                    .SetModule(request.GetModule())
                    .SetMsgUuid(request.GetMsgUuid())
                    .SetRoute(request.GetRoute());

        // 根据请求类型设置响应类型
        switch (request.GetMessageType())
        {
        case MessageType::kRequestAsync:
            response_ptr->SetMessageType(MessageType::kResponseAsync);
            break;
        case MessageType::kRequestAsyncFunc:
            response_ptr->SetMessageType(MessageType::kResponseAsyncFunc);
            break;
        case MessageType::kRequestAsyncFuture:
            response_ptr->SetMessageType(MessageType::kResponseAsyncFuture);
            break;
        default:
            break;
        }

        const auto [fst, snd] = context::ContextFactory::WithTimeout(context::ContextFactory::Background(),
                                                                     std::chrono::milliseconds(request.GetTimeout()));
        const auto timer_ctx = std::dynamic_pointer_cast<tyke::TimerContext>(fst);
        if (!timer_ctx)
        {
            LOG_ERROR("Failed to cast context to TimerContext");
            response_ptr->SetResult(StatusCode::kInternalError, "internal error");
            if (auto send_result = response_ptr->SendAsync(); !send_result)
            {
                LOG_ERROR("Send async response failed: {}", send_result.error());
            }
            return;
        }
        auto token = timer_ctx->RegisterCallback(
            [response_ptr]()
            {
                response_ptr->SetResult(StatusCode::kTimeout, "timeout");
                if (auto send_result = response_ptr->SendAsync(); !send_result)
                {
                    LOG_ERROR("Send async response failed: {}", send_result.error());
                }
            });
        timer_ctx->ActivateTimer();

        dispatcher::DispatchRequest(request, *response_ptr, fst);

        if (!response_ptr->IsSent())
        {
            timer_ctx->UnregisterCallback(token);
            if (auto send_result = response_ptr->SendAsync(); !send_result)
            {
                LOG_ERROR("Send async response failed: {}", send_result.error());
            }
        }
    }

    void ResponseHandler(const Response& response)
    {
        LOG_DEBUG("ResponseHandler: route={}, msg_uuid={}, msg_type={}", response.GetRoute(), response.GetMsgUuid(),
                  static_cast<int>(response.GetMessageType()));

        switch (response.GetMessageType())
        {
        case MessageType::kResponseAsync:
            // 分发到响应处理器
            dispatcher::DispatchResponse(response);
            break;
        case MessageType::kResponseAsyncFunc:
            // 执行回调函数
            stub::ExecFunc(response);
            break;
        case MessageType::kResponseAsyncFuture:
            // 设置Future结果
            stub::SetFuture(response);
            break;
        default:
            LOG_WARN("Unknown response type: {}", static_cast<int>(response.GetMessageType()));
            break;
        }
    }
} // namespace tyke::data_handler
