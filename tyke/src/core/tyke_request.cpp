#include "core/tyke_request.h"

#include <unordered_map>

#include "common/log_def.h"
#include "common/tyke_utils.h"
#include "core/data_proc.h"
#include "core/request_stub.h"
#include "ipc/ipc_client.h"

namespace tyke
{
    void TykeRequest::Reset()
    {
        protocol_header_ = ProtocolHeader{};
        metadata_ = RequestMetadata{};
        content_.clear();
    }

    const char* TykeRequest::GetMagic() const
    {
        return protocol_header_.magic;
    }

    MessageType TykeRequest::GetMessageType() const
    {
        return static_cast<MessageType>(protocol_header_.msg_type);
    }

    void TykeRequest::GetContent(std::string& content_type,
                                 std::vector<uint8_t>& content) const
    {
        content_type = metadata_.GetContentType();
        content = content_;
    }

    TykeRequest& TykeRequest::SetContent(const ContentType& content_type,
                                         const std::vector<uint8_t>& content)
    {
        metadata_.SetContentType(ContentTypeMap().at(content_type));
        content_ = content;
        return *this;
    }

    TykeRequest& TykeRequest::SetModule(const std::string_view module)
    {
        metadata_.SetModule(module);
        return *this;
    }

    const std::string& TykeRequest::GetModule() const
    {
        return metadata_.GetModule();
    }

    TykeRequest& TykeRequest::SetRoute(const std::string_view route)
    {
        metadata_.SetRoute(route);
        return *this;
    }

    const std::string& TykeRequest::GetRoute() const
    {
        return metadata_.GetRoute();
    }

    BoolResult TykeRequest::EncodeAndSend(const std::string& send_uuid,
                                          MessageType msg_type,
                                          uint32_t timeout_ms)
    {
        LOG_DEBUG("EncodeAndSend: send_uuid={}, route={}, msg_type={}, timeout={}ms",
                  send_uuid, GetRoute(), static_cast<int>(msg_type), timeout_ms);

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

    BoolResult TykeRequest::Send(const std::string& send_uuid, TykeResponse& response,
                                 uint32_t timeout_ms)
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

        auto send_result = IpcClient::Send(send_uuid, data_vec,
                                           [&response](const std::vector<uint8_t>& recv_data) -> bool
                                           {
                                               uint32_t data_size = 0;
                                               const auto decode_result = DataProc::DecodeResponse(
                                                   recv_data, response, data_size);
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

    BoolResult TykeRequest::SendAsync(const std::string& send_uuid,
                                      const uint32_t timeout_ms)
    {
        return EncodeAndSend(send_uuid, MessageType::kRequestAsync, timeout_ms);
    }

    BoolResult TykeRequest::SendAsyncWithFunc(
        const std::string& send_uuid,
        const std::function<void(const TykeResponse &)>& func,
        uint32_t timeout_ms)
    {
        LOG_DEBUG("SendAsyncWithFunc: send_uuid={}, route={}, timeout={}ms",
                  send_uuid, GetRoute(), timeout_ms);

        auto result = EncodeAndSend(send_uuid, MessageType::kRequestAsyncFunc, timeout_ms);
        if (!result)
        {
            LOG_ERROR("Send request failed: {}", result.error());
            return nonstd::make_unexpected("encode request failed");
        }
        stub::AddFunc(metadata_.GetMsgUuid(), func);
        return result;
    }

    nonstd::expected<ResponseFuture, std::string> TykeRequest::SendAsyncWithFuture(
        const std::string& send_uuid, uint32_t timeout_ms)
    {
        LOG_DEBUG("SendAsyncWithFuture: send_uuid={}, route={}, timeout={}ms",
                  send_uuid, GetRoute(), timeout_ms);

        if (auto result = EncodeAndSend(send_uuid, MessageType::kRequestAsyncFuture, timeout_ms);
            !result)
        {
            return nonstd::make_unexpected(result.error());
        }

        std::promise<TykeResponse> promise;
        auto future = promise.get_future();
        stub::AddFuture(metadata_.GetMsgUuid(), promise);
        ResponseFuture response_future(metadata_.GetMsgUuid(), std::move(future));

        LOG_DEBUG("Future registered, msg_uuid={}", GetMsgUuid());
        return response_future;
    }

    std::optional<bool> TykeRequest::AddMetadata(const std::string_view key,
                                                 const JsonValue& value)
    {
        return metadata_.AddMetadata(key, value);
    }

    std::optional<JsonValue> TykeRequest::GetMetadata(const std::string_view key) const
    {
        return metadata_.GetMetadata(key);
    }

    const std::string& TykeRequest::GetMsgUuid() const
    {
        return metadata_.GetMsgUuid();
    }

    TykeRequest& TykeRequest::SetAsyncUuid(const std::string_view async_uuid)
    {
        metadata_.SetAsyncUuid(async_uuid);
        return *this;
    }

    const std::string& TykeRequest::GetAsyncUuid() const
    {
        return metadata_.GetAsyncUuid();
    }

    TykeRequest& TykeRequest::SetTimeout(const uint64_t timeout)
    {
        metadata_.SetTimeout(timeout);
        return *this;
    }

    uint64_t TykeRequest::GetTimeout() const
    {
        return metadata_.GetTimeout();
    }
} // namespace tyke