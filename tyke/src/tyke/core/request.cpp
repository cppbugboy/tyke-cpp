/**
 * @file request.cpp
 * @brief 请求消息实现，包含请求的构造、编码发送（同步/异步/回调/Future）以及元数据访问。
 * @author Nick
 * @date 2026/04/19
 */

#include "tyke/core/request.h"

#include <unordered_map>

#include "tyke/common/log_def.h"
#include "tyke/common/tyke_utils.h"
#include "tyke/core/data_proc.h"
#include "tyke/core/request_stub.h"
#include "tyke/ipc/ipc_client.h"

namespace tyke
{
    /** @brief 重置请求对象至初始状态，供对象池复用。 */
    void Request::Reset()
    {
        protocol_header_ = ProtocolHeader{};
        metadata_ = RequestMetadata{};
        content_.clear();
    }

    const char* Request::GetMagic() const
    {
        return protocol_header_.magic;
    }

    MessageType Request::GetMessageType() const
    {
        return static_cast<MessageType>(protocol_header_.msg_type);
    }

    void Request::GetContent(std::string& content_type, std::vector<uint8_t>& content) const
    {
        content_type = metadata_.GetContentType();
        content = content_;
    }

    Request& Request::SetContent(const ContentType& content_type, const std::vector<uint8_t>& content)
    {
        metadata_.SetContentType(ContentTypeMap().at(content_type));
        content_ = content;
        return *this;
    }

    Request& Request::SetContent(const ContentType& content_type, const std::string& content)
    {
        return SetContent(content_type, std::vector<uint8_t>(content.begin(), content.end()));
    }

    void Request::GetContent(std::string& content_type, std::string& content) const
    {
        std::vector<uint8_t> content_vec;
        GetContent(content_type, content_vec);
        content = std::string(content_vec.begin(), content_vec.end());
    }

    Request& Request::SetModule(const std::string_view module)
    {
        metadata_.SetModule(module);
        return *this;
    }

    const std::string& Request::GetModule() const
    {
        return metadata_.GetModule();
    }

    Request& Request::SetRoute(const std::string_view route)
    {
        metadata_.SetRoute(route);
        return *this;
    }

    const std::string& Request::GetRoute() const
    {
        return metadata_.GetRoute();
    }

    /**
     * @brief 编码请求并通过 IPC 发送。
     *
     * 设置 msg_uuid 和时间戳，将请求编码为协议帧，通过 IpcClient::SendAsync 异步发送。
     *
     * @param send_uuid 目标服务端 UUID
     * @param msg_type 消息类型（kRequest / kRequestAsync / kRequestAsyncFunc / kRequestAsyncFuture）
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult Request::EncodeAndSend(const std::string& send_uuid, MessageType msg_type, uint32_t timeout_ms)
    {
        LOG_DEBUG("EncodeAndSend: send_uuid={}, route={}, msg_type={}, timeout={}ms", send_uuid, GetRoute(),
                  static_cast<int>(msg_type), timeout_ms);

        metadata_.SetTimeout(timeout_ms);
        protocol_header_.msg_type = msg_type;
        metadata_.SetMsgUuid(utils::GenerateUUID()).SetTimestamp(utils::GenerateTimestamp());

        std::vector<uint8_t> data_vec;
        try
        {
            DataProc::EncodeRequest(*this, data_vec);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Encode request failed: {}", e.what());
            return nonstd::make_unexpected("encode request failed");
        }

        auto send_result = IpcClient::SendAsync(send_uuid, data_vec, timeout_ms);
        if (!send_result)
        {
            LOG_ERROR("Send request failed: {}", send_result.error());
            return nonstd::make_unexpected("send request failed: " + send_result.error());
        }

        LOG_DEBUG("Request sent successfully, msg_uuid={}", GetMsgUuid());
        return true;
    }

    /**
     * @brief 同步发送请求并等待响应。
     *
     * 将请求编码后通过 IpcClient::Send 同步发送，传入解码回调在收到数据时解析响应。
     * 调用会阻塞直到收到完整响应或超时。
     *
     * @param send_uuid 目标服务端 UUID
     * @param response [out] 接收到的响应对象
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult Request::Send(const std::string& send_uuid, Response& response, uint32_t timeout_ms)
    {
        LOG_DEBUG("Send: send_uuid={}, route={}, timeout={}ms", send_uuid, GetRoute(), timeout_ms);

        metadata_.SetTimeout(timeout_ms);
        protocol_header_.msg_type = MessageType::kRequest;
        metadata_.SetMsgUuid(utils::GenerateUUID()).SetTimestamp(utils::GenerateTimestamp());

        std::vector<uint8_t> data_vec;
        try
        {
            DataProc::EncodeRequest(*this, data_vec);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Encode request failed: {}", e.what());
            return nonstd::make_unexpected("encode request failed");
        }

        auto send_result = IpcClient::Send(
            send_uuid, data_vec,
            [&response](const std::vector<uint8_t>& recv_data) -> bool
            {
                uint32_t data_size = 0;
                const auto decode_result = DataProc::DecodeResponse(recv_data, response, data_size);
                return decode_result.has_value();
            },
            timeout_ms);
        if (!send_result)
        {
            LOG_ERROR("Send request failed: {}", send_result.error());
            return nonstd::make_unexpected("send request failed: " + send_result.error());
        }

        LOG_DEBUG("Sync request completed, msg_uuid={}", GetMsgUuid());
        return true;
    }

    /** @brief 异步即发即弃请求，不等待响应。 */
    BoolResult Request::SendAsync(const std::string& send_uuid, const uint32_t timeout_ms)
    {
        return EncodeAndSend(send_uuid, MessageType::kRequestAsync, timeout_ms);
    }

    /**
     * @brief 异步发送请求并在收到响应时执行回调函数。
     *
     * 将回调注册到 stub 映射中，当响应到达时由 ResponseHandler 调用 ExecFunc 执行。
     *
     * @param send_uuid 目标服务端 UUID
     * @param func 收到响应时执行的回调函数
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回 true，失败返回错误信息。
     */
    BoolResult Request::SendAsyncWithFunc(const std::string& send_uuid,
                                          const std::function<void(const Response&)>& func,
                                          uint32_t timeout_ms)
    {
        LOG_DEBUG("SendAsyncWithFunc: send_uuid={}, route={}, timeout={}ms", send_uuid, GetRoute(), timeout_ms);

        auto result = EncodeAndSend(send_uuid, MessageType::kRequestAsyncFunc, timeout_ms);
        if (!result)
        {
            LOG_ERROR("Send request failed: {}", result.error());
            return nonstd::make_unexpected("encode request failed");
        }
        stub::AddFunc(metadata_.GetMsgUuid(), func);
        return result;
    }

    /**
     * @brief 异步发送请求并返回 std::future<Response>。
     *
     * 创建 promise/future 对并注册到 stub 映射中，当响应到达时由 ResponseHandler 调用 SetFuture 设置结果。
     *
     * @param send_uuid 目标服务端 UUID
     * @param timeout_ms 超时时间（毫秒）
     * @return 包含 future<Response> 的 expected，调用方可通过 future.get() 阻塞等待结果。
     */
    nonstd::expected<std::future<Response>, std::string> Request::SendAsyncWithFuture(const std::string& send_uuid,
        uint32_t timeout_ms)
    {
        LOG_DEBUG("SendAsyncWithFuture: send_uuid={}, route={}, timeout={}ms", send_uuid, GetRoute(), timeout_ms);

        if (auto result = EncodeAndSend(send_uuid, MessageType::kRequestAsyncFuture, timeout_ms); !result)
        {
            return nonstd::make_unexpected(result.error());
        }

        std::promise<Response> promise;
        auto future = promise.get_future();
        stub::AddFuture(metadata_.GetMsgUuid(), promise, timeout_ms);

        LOG_DEBUG("Future registered, msg_uuid={}", GetMsgUuid());
        return future;
    }

    Request& Request::SetContext(const std::shared_ptr<Context>& context)
    {
        context_ = context;
        return *this;
    }

    std::shared_ptr<Context> Request::GetContext() const
    {
        return context_;
    }

    std::optional<bool> Request::AddMetadata(const std::string_view key, const JsonValue& value)
    {
        return metadata_.AddMetadata(key, value);
    }

    std::optional<JsonValue> Request::GetMetadata(const std::string_view key) const
    {
        return metadata_.GetMetadata(key);
    }

    const std::string& Request::GetMsgUuid() const
    {
        return metadata_.GetMsgUuid();
    }

    Request& Request::SetAsyncUuid(const std::string_view async_uuid)
    {
        metadata_.SetAsyncUuid(async_uuid);
        return *this;
    }

    const std::string& Request::GetAsyncUuid() const
    {
        return metadata_.GetAsyncUuid();
    }

    Request& Request::SetTimeout(const uint64_t timeout)
    {
        metadata_.SetTimeout(timeout);
        return *this;
    }

    uint64_t Request::GetTimeout() const
    {
        return metadata_.GetTimeout();
    }
} // namespace tyke
