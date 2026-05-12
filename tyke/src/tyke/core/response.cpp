#include "tyke/core/response.h"

#include "tyke/common/log_def.h"
#include "tyke/common/tyke_utils.h"
#include "tyke/core/data_proc.h"
#include "tyke/ipc/ipc_client.h"

namespace tyke
{
    void Response::Reset()
    {
        state_->is_send.store(false, std::memory_order_release);
        protocol_header_ = ProtocolHeader{};
        metadata_ = ResponseMetadata{};
        content_.clear();
        client_id_ = {};
        send_data_handler_ = {};
    }

    const char* Response::GetMagic() const
    {
        return protocol_header_.magic;
    }

    const std::string& Response::GetMsgUuid() const
    {
        return metadata_.GetMsgUuid();
    }

    Response& Response::SetRoute(const std::string_view route)
    {
        metadata_.SetRoute(route);
        return *this;
    }

    const std::string& Response::GetRoute() const
    {
        return metadata_.GetRoute();
    }

    Response& Response::SetContent(const ContentType& content_type, const std::vector<uint8_t>& response_content)
    {
        metadata_.SetContentType(ContentTypeMap().at(content_type));
        content_ = response_content;
        return *this;
    }

    Response& Response::SetContent(const ContentType& content_type, const std::string& content)
    {
        return SetContent(content_type, std::vector<uint8_t>(content.begin(), content.end()));
    }

    void Response::GetContent(std::string& content_type, std::string& content) const
    {
        std::vector<uint8_t> content_vec;
        GetContent(content_type, content_vec);
        content = std::string(content_vec.begin(), content_vec.end());
    }

    Response& Response::SetMessageType(const MessageType msg_type)
    {
        protocol_header_.msg_type = msg_type;
        return *this;
    }

    MessageType Response::GetMessageType() const
    {
        return static_cast<MessageType>(protocol_header_.msg_type);
    }

    Response& Response::SetModule(const std::string_view module)
    {
        metadata_.SetModule(module);
        return *this;
    }

    const std::string& Response::GetModule() const
    {
        return metadata_.GetModule();
    }

    Response& Response::SetMsgUuid(const std::string_view msg_uuid)
    {
        metadata_.SetMsgUuid(msg_uuid);
        return *this;
    }

    void Response::GetContent(std::string& content_type, std::vector<uint8_t>& content) const
    {
        content_type = metadata_.GetContentType();
        content = content_;
    }

    std::optional<bool> Response::AddMetadata(const std::string_view key, const JsonValue& value)
    {
        return metadata_.AddMetadata(key, value);
    }

    std::optional<JsonValue> Response::GetMetadata(const std::string_view key) const
    {
        return metadata_.GetMetadata(key);
    }

    Response& Response::SetResult(const StatusCode status, const std::string_view reason)
    {
        metadata_.SetStatus(status).SetReason(reason);
        return *this;
    }

    void Response::GetResult(StatusCode& status, std::string& reason) const
    {
        status = metadata_.GetStatus();
        reason = metadata_.GetReason();
    }

    BoolResult Response::Send()
    {
        LOG_DEBUG("Send: route={}, msg_uuid={}", GetRoute(), GetMsgUuid());

        bool expected = false;
        if (!state_->is_send.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                                     std::memory_order_acquire))
        {
            LOG_WARN("Response already sent, msg_uuid={}", GetMsgUuid());
            return nonstd::make_unexpected("response already sent");
        }

        if (!send_data_handler_)
        {
            LOG_ERROR("Send data handler is not set, msg_uuid={}", GetMsgUuid());
            state_->is_send.store(false, std::memory_order_release);
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
            state_->is_send.store(false, std::memory_order_release);
            return nonstd::make_unexpected("encode response failed");
        }

        if (!send_data_handler_(client_id_, data_vec))
        {
            LOG_ERROR("Send data handler failed, msg_uuid={}", GetMsgUuid());
            state_->is_send.store(false, std::memory_order_release);
            return nonstd::make_unexpected("send data handler failed");
        }

        LOG_DEBUG("Response sent successfully, msg_uuid={}", GetMsgUuid());
        return true;
    }

    BoolResult Response::SendAsync()
    {
        LOG_DEBUG("SendAsync: route={}, msg_uuid={}, async_uuid={}", GetRoute(), GetMsgUuid(),
                  metadata_.GetAsyncUuid());

        bool expected = false;
        if (!state_->is_send.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                                     std::memory_order_acquire))
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
            state_->is_send.store(false, std::memory_order_release);
            return nonstd::make_unexpected("encode response failed");
        }

        if (auto send_result = IpcClient::SendAsync(metadata_.GetAsyncUuid(), data_vec); !send_result)
        {
            LOG_ERROR("Send async failed: {}", send_result.error());
            state_->is_send.store(false, std::memory_order_release);
            return nonstd::make_unexpected("send async failed: " + send_result.error());
        }

        LOG_DEBUG("Async response sent successfully, msg_uuid={}", GetMsgUuid());
        return true;
    }

    Response& Response::SetAsyncUuid(const std::string_view target_uuid)
    {
        metadata_.SetAsyncUuid(target_uuid);
        return *this;
    }

    const std::string& Response::GetAsyncUuid() const
    {
        return metadata_.GetAsyncUuid();
    }

    Response& Response::SetSendDataHandler(const SendDataHandler& send_data_handler)
    {
        send_data_handler_ = send_data_handler;
        return *this;
    }

    Response& Response::SetClientId(const ClientId client_id)
    {
        client_id_ = client_id;
        return *this;
    }

    bool Response::IsSent() const
    {
        return state_->is_send.load(std::memory_order_acquire);
    }
} // namespace tyke
