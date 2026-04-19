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
    TykeRequest* TykeRequest::Acquire()
    {
        LOG_DEBUG("Acquiring request object from pool");
        return pool_.Acquire();
    }
    void TykeRequest::Release(TykeRequest* req)
    {
        if (req)
        {
            LOG_DEBUG("Releasing request object to pool, msg_uuid={}", req->GetMsgUuid());
            req->Reset();
            pool_.Release(req);
        }
    }
    const char* TykeRequest::GetMagic() const
    {
        return protocol_header_.magic;
    }
    MessageType TykeRequest::GetMessageType() const
    {
        return static_cast<MessageType>(protocol_header_.msg_type);
    }
    void TykeRequest::GetContent(std::string& content_type, std::vector<unsigned char>& content) const
    {
        content_type = metadata_.GetContentType();
        content = content_;
    }
    const std::string& TykeRequest::GetModule() const
    {
        return metadata_.GetModule();
    }
    const std::string& TykeRequest::GetRoute() const
    {
        return metadata_.GetRoute();
    }
    TykeRequest& TykeRequest::SetContent(const ContentType& content_type, const std::vector<unsigned char>& content)
    {
        metadata_.SetContentType(ContentTypeMap().at(content_type));
        content_ = content;
        return *this;
    }
    TykeRequest& TykeRequest::SetModule(std::string_view module)
    {
        metadata_.SetModule(module);
        return *this;
    }
    TykeRequest& TykeRequest::SetRoute(std::string_view route)
    {
        metadata_.SetRoute(route);
        return *this;
    }
    nonstd::expected<bool, std::string> TykeRequest::EncodeAndSend(const std::string& send_uuid, MessageType msg_type)
    {
        LOG_DEBUG("EncodeAndSend: send_uuid={}, route={}, msg_type={}",
                  send_uuid, GetRoute(), static_cast<int>(msg_type));

        protocol_header_.msg_type = msg_type;
        metadata_.SetMsgUuid(utils::GenerateUUID()).SetTimestamp(utils::GenerateTimestamp());

        std::vector<unsigned char> data_vec;
        try
        {
            DataProc::EncodeRequest(*this, data_vec);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Encode request failed: {}", e.what());
            return nonstd::make_unexpected("encode request failed");
        }

        auto send_result = IpcClient::SendAsync(send_uuid, data_vec);
        if (!send_result)
        {
            LOG_ERROR("Send request failed: {}", send_result.error());
            return nonstd::make_unexpected("send request failed: " + send_result.error());
        }

        LOG_DEBUG("Request sent successfully, msg_uuid={}", GetMsgUuid());
        return true;
    }
    nonstd::expected<bool, std::string> TykeRequest::Send(const std::string& send_uuid, TykeResponse& response)
{
    LOG_DEBUG("Send: send_uuid={}, route={}", send_uuid, GetRoute());

    protocol_header_.msg_type = MessageType::kRequest;
    metadata_.SetMsgUuid(utils::GenerateUUID()).SetTimestamp(utils::GenerateTimestamp());

    std::vector<unsigned char> data_vec;
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
                                      [&response](const std::vector<unsigned char>& recv_data) -> bool
                                      {
                                          uint32_t data_size = 0;
                                           const auto decode_result = DataProc::DecodeResponse(recv_data, response, data_size);
                                          return decode_result.has_value();
                                      });
        if (!send_result)
        {
            LOG_ERROR("Send request failed: {}", send_result.error());
            return nonstd::make_unexpected("send request failed: " + send_result.error());
        }

        LOG_DEBUG("Sync request completed, msg_uuid={}", GetMsgUuid());
        return true;
}
    nonstd::expected<bool, std::string> TykeRequest::SendAsync(const std::string& send_uuid,
                                                               const std::string&)
    {
        return EncodeAndSend(send_uuid, MessageType::kRequestAsync);
    }
    nonstd::expected<bool, std::string> TykeRequest::SendAsyncWithFunc(const std::string& send_uuid, const std::function<void(const TykeResponse &)> &func)
    {
        LOG_DEBUG("SendAsyncWithFunc: send_uuid={}, route={}", send_uuid, GetRoute());

        auto result = EncodeAndSend(send_uuid, MessageType::kRequestAsyncFunc);
        if (result)
        {
            RequestStub::AddFunc(metadata_.GetMsgUuid(), func);
            LOG_DEBUG("Async callback registered, msg_uuid={}", GetMsgUuid());
        }
        return result;
    }
    nonstd::expected<ResponseFuture, std::string> TykeRequest::SendAsyncWithFuture(const std::string& send_uuid,
        const std::string&)
    {
        LOG_DEBUG("SendAsyncWithFuture: send_uuid={}, route={}", send_uuid, GetRoute());

        auto result = EncodeAndSend(send_uuid, MessageType::kRequestAsyncFuture);
        if (!result)
        {
            return nonstd::make_unexpected(result.error());
        }

        std::promise<TykeResponse> promise;
        RequestStub::AddFuture(metadata_.GetMsgUuid(), promise);
        ResponseFuture response_future(metadata_.GetMsgUuid(), promise.get_future());
        LOG_DEBUG("Future registered, msg_uuid={}", GetMsgUuid());
        return response_future;
    }
    nonstd::expected<bool, std::string> TykeRequest::AddMetadata(const std::string_view key, const JsonValue& value)
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
    TykeRequest &TykeRequest::SetAsyncUuid(std::string_view async_uuid)
    {
        metadata_.SetAsyncUuid(async_uuid);
        return *this;
    }
    const std::string& TykeRequest::GetAsyncUuid() const
    {
        return metadata_.GetAsyncUuid();
    }
} // tyke
