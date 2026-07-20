/**
 * @file data_handler.cpp
 * @brief 数据处理器实现。接收 IPC 层解析后的原始数据，按消息类型分发到同步/异步请求处理器或响应处理器。
 * @author Nick
 * @date 2026/04/17
 */

#include "tyke/core/data_handler.h"

#include <nlohmann/json.hpp>

#include "tyke/common/log_def.h"
#include "tyke/component/timing_wheel.h"
#include "tyke/core/context_factory.h"
#include "tyke/core/data_proc.h"
#include "tyke/core/dispatcher.h"
#include "tyke/core/request.h"
#include "tyke/core/request_stub.h"
#include "tyke/core/response.h"

namespace tyke::data_handler
{
    /**
     * @brief IPC 数据回调入口。
     *
     * 从 IPC 层接收原始数据，解析协议头判定消息类型，然后路由到相应的处理器。
     *
     * 消息类型路由：
     * - kRequest -> RequestHandler（同步请求-响应模式）
     * - kRequestAsync / kRequestAsyncFunc / kRequestAsyncFuture -> RequestHandlerAsync（异步模式）
     * - kResponseAsync* -> ResponseHandler（异步响应分发）
     *
     * @param client_id 客户端标识
     * @param data_vec 原始数据缓冲区
     * @param send_data_handler 发送数据的回调（服务端用于 Send 响应回客户端）
     * @return 消费的字节数；nullopt 表示数据不完整需等待更多数据；0 表示数据损坏应丢弃。
     *
     * @note 数据不足 sizeof(ProtocolHeader) 或魔数不匹配时返回 0 丢弃。
     * @note 解码失败且 used==0 时返回 nullopt 等待更多数据；used>0 时返回 0 丢弃损坏数据。
     */
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
                return 0;
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
                        ResponseHandler(std::move(*tyke_response_ptr));
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

    /**
     * @brief 同步请求处理器。
     *
     * 创建 TimerContext 设置超时回调，将请求上下文与响应绑定后分发到业务处理链。
     * 若处理器未发送响应，则自动发送（取消超时回调后发送）。
     *
     * @param request 解码后的请求对象
     * @param client_id 客户端标识（用于写回响应）
     * @param send_data_handler 发送响应数据的回调
     */
    void RequestHandler(Request& request, const ClientId client_id, const SendDataHandler& send_data_handler)
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

        // 创建带超时的上下文
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
            snd();
            return;
        }
        // 注册超时回调：到期自动发送超时响应
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
        request.SetContext(timer_ctx);

        dispatcher::DispatchRequest(request, *response_ptr);

        // 若业务处理未显式发送响应，则取消超时回调后自动发送
        if (!response_ptr->IsSent())
        {
            timer_ctx->UnregisterCallback(token);
            if (auto send_result = response_ptr->Send(); !send_result)
            {
                LOG_ERROR("Send response failed: {}", send_result.error());
            }
        }
        snd();
    }

    /**
     * @brief 异步请求处理器。
     *
     * 与 RequestHandler 类似，但为异步模式：根据请求类型设置对应响应消息类型，
     * 响应通过 SendAsync 发送回请求方的 listen_uuid。
     *
     * @param request 解码后的异步请求对象
     */
    void RequestHandlerAsync(Request& request)
    {
        LOG_DEBUG("RequestHandlerAsync: route={}, msg_uuid={}", request.GetRoute(), request.GetMsgUuid());

        const auto response_ptr = Response::Acquire();
        response_ptr->SetAsyncUuid(request.GetAsyncUuid())
                    .SetMessageType(MessageType::kResponseAsync)
                    .SetModule(request.GetModule())
                    .SetMsgUuid(request.GetMsgUuid())
                    .SetRoute(request.GetRoute());

        // 根据请求类型设置对应的响应类型，确保请求-响应对的类型一致
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
            snd();
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
        request.SetContext(timer_ctx);

        dispatcher::DispatchRequest(request, *response_ptr);

        if (!response_ptr->IsSent())
        {
            timer_ctx->UnregisterCallback(token);
            if (auto send_result = response_ptr->SendAsync(); !send_result)
            {
                LOG_ERROR("Send async response failed: {}", send_result.error());
            }
        }
        snd();
    }

    /**
     * @brief 异步响应处理器。
     *
     * 根据响应的消息类型分发：
     * - kResponseAsync -> dispatcher::DispatchResponse（路由到业务响应处理器）
     * - kResponseAsyncFunc -> stub::ExecFunc（执行注册的回调函数）
     * - kResponseAsyncFuture -> stub::SetFuture（设置 future 结果唤醒等待者）
     *
     * @param response 解码后的响应对象（值传递，转移所有权）
     */
    void ResponseHandler(Response response)
    {
        LOG_DEBUG("ResponseHandler: route={}, msg_uuid={}, msg_type={}", response.GetRoute(), response.GetMsgUuid(),
                  static_cast<int>(response.GetMessageType()));

        switch (response.GetMessageType())
        {
        case MessageType::kResponseAsync:
            dispatcher::DispatchResponse(response);
            break;
        case MessageType::kResponseAsyncFunc:
            stub::ExecFunc(response);
            break;
        case MessageType::kResponseAsyncFuture:
            stub::SetFuture(std::move(response));
            break;
        default:
            LOG_WARN("Unknown response type: {}", static_cast<int>(response.GetMessageType()));
            break;
        }
    }
} // namespace tyke::data_handler
