#include "core/tyke_response.h"

#include "common/log_def.h"
#include "common/tyke_utils.h"
#include "core/data_proc.h"
#include "ipc/ipc_client.h"

namespace tyke
{
    TykeResponse::TykeResponse() = default;

    void TykeResponse::Reset()
    {
        protocol_header_ = ProtocolHeader{};
        metadata_ = ResponseMetadata{};
        content_.clear();
        is_send_ = false;
        client_id_ = {};
        send_data_handler_ = nullptr;
    }

    const char* TykeResponse::GetMagic() const
    {
        return protocol_header_.magic;
    }

    const std::string& TykeResponse::GetMsgUuid() const
    {
        return metadata_.GetMsgUuid();
    }

    TykeResponse& TykeResponse::SetRoute(const std::string_view route)
    {
        metadata_.SetRoute(route);
        return *this;
    }

    const std::string& TykeResponse::GetRoute() const
    {
        return metadata_.GetRoute();
    }

    TykeResponse& TykeResponse::SetContent(const ContentType& content_type,
                                           const std::vector<uint8_t>& response_content)
    {
        metadata_.SetContentType(ContentTypeMap().at(content_type));
        content_ = response_content;
        return *this;
    }

    TykeResponse& TykeResponse::SetMessageType(const MessageType msg_type)
    {
        protocol_header_.msg_type = msg_type;
        return *this;
    }

    MessageType TykeResponse::GetMessageType() const
    {
        return static_cast<MessageType>(protocol_header_.msg_type);
    }

    TykeResponse& TykeResponse::SetModule(const std::string_view module)
    {
        metadata_.SetModule(module);
        return *this;
    }

    const std::string& TykeResponse::GetModule() const
    {
        return metadata_.GetModule();
    }

    TykeResponse& TykeResponse::SetMsgUuid(const std::string_view msg_uuid)
    {
        metadata_.SetMsgUuid(msg_uuid);
        return *this;
    }

    void TykeResponse::GetContent(std::string& content_type,
                                  std::vector<uint8_t>& content) const
    {
        content_type = metadata_.GetContentType();
        content = content_;
    }

    std::optional<bool> TykeResponse::AddMetadata(const std::string_view key,
                                                  const JsonValue& value)
    {
        return metadata_.AddMetadata(key, value);
    }

    std::optional<JsonValue> TykeResponse::GetMetadata(const std::string_view key) const
    {
        return metadata_.GetMetadata(key);
    }

    TykeResponse& TykeResponse::SetResult(const StatusCode status,
                                          const std::string_view reason)
    {
        metadata_.SetStatus(status).SetReason(reason);
        return *this;
    }

    void TykeResponse::GetResult(StatusCode& status, std::string& reason) const
    {
        status = metadata_.GetStatus();
        reason = metadata_.GetReason();
    }

    BoolResult TykeResponse::Send()
    {
        LOG_DEBUG("Send: route={}, msg_uuid={}", GetRoute(), GetMsgUuid());

        if (is_send_)
        {
            LOG_WARN("Response already sent, msg_uuid={}", GetMsgUuid());
            return nonstd::make_unexpected("response already sent");
        }

        if (!send_data_handler_)
        {
            LOG_ERROR("Send data handler is not set, msg_uuid={}", GetMsgUuid());
            return nonstd::make_unexpected("send data handler is not set");
        }

        metadata_.SetTimestamp(utils::GenerateTimestamp());
        std::vector<uint8_t> data_vec;
        try
        {
            DataProc::EncodeResponse(*this, data_vec);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Encode response failed: {}", e.what());
            return nonstd::make_unexpected("encode response failed");
        }

        if (!send_data_handler_(client_id_, data_vec))
        {
            LOG_ERROR("Send data handler failed, msg_uuid={}", GetMsgUuid());
            return nonstd::make_unexpected("send data handler failed");
        }

        is_send_ = true;
        LOG_DEBUG("Response sent successfully, msg_uuid={}", GetMsgUuid());
        return true;
    }

    BoolResult TykeResponse::SendAsync()
    {
        LOG_DEBUG("SendAsync: route={}, msg_uuid={}, async_uuid={}",
                  GetRoute(), GetMsgUuid(), metadata_.GetAsyncUuid());

        if (is_send_)
        {
            LOG_WARN("Response already sent, msg_uuid={}", GetMsgUuid());
            return nonstd::make_unexpected("response already sent");
        }

        metadata_.SetTimestamp(utils::GenerateTimestamp());
        std::vector<uint8_t> data_vec;
        try
        {
            DataProc::EncodeResponse(*this, data_vec);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Encode response failed: {}", e.what());
            return nonstd::make_unexpected("encode response failed");
        }

        if (auto send_result = IpcClient::SendAsync(metadata_.GetAsyncUuid(), data_vec);
            !send_result)
        {
            LOG_ERROR("Send async failed: {}", send_result.error());
            return nonstd::make_unexpected("send async failed: " + send_result.error());
        }

        is_send_ = true;
        LOG_DEBUG("Async response sent successfully, msg_uuid={}", GetMsgUuid());
        return true;
    }

    TykeResponse& TykeResponse::SetAsyncUuid(const std::string_view target_uuid)
    {
        metadata_.SetAsyncUuid(target_uuid);
        return *this;
    }

    const std::string& TykeResponse::GetAsyncUuid() const
    {
        return metadata_.GetAsyncUuid();
    }

    TykeResponse& TykeResponse::SetSendDataHandler(const SendDataHandler& send_data_handler)
    {
        send_data_handler_ = send_data_handler;
        return *this;
    }

    TykeResponse& TykeResponse::SetClientId(const ClientId client_id)
    {
        client_id_ = client_id;
        return *this;
    }
} // namespace tyke
