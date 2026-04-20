/**
 * @file tyke_request.h
 * @brief 请求对象声明。封装IPC请求的元数据、内容和发送功能，支持同步和异步发送。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <future>
#include <string_view>

#include "common/tyke_result.h"
#include "request_metadata.h"
#include "tyke_response.h"
#include "response_future.h"
#include "common/tyke_def.h"
#include "ipc/ipc_types.h"
#include "component/object_pool.h"

namespace tyke
{
    
    class TykeRequest
    {
        friend class DataProc;

    public:
        
        static TykeRequest* Acquire();

        
        static void Release(TykeRequest* req);

        
        void Reset();

        
        const char* GetMagic() const;

        
        MessageType GetMessageType() const;

        
        TykeRequest& SetContent(const ContentType& content_type, const std::vector<unsigned char>& content);


        void GetContent(std::string& content_type, std::vector<unsigned char>& content) const;


        TykeRequest& SetModule(std::string_view module);


        const std::string& GetModule() const;


        TykeRequest& SetRoute(std::string_view route);


        const std::string& GetRoute() const;

        
        const std::string& GetMsgUuid() const;

        TykeRequest& SetAsyncUuid(std::string_view async_uuid);
        
        const std::string& GetAsyncUuid() const;

        
        nonstd::expected<bool, std::string> AddMetadata(std::string_view key, const JsonValue& value);


        std::optional<JsonValue> GetMetadata(std::string_view key) const;

        
        nonstd::expected<bool, std::string> Send(const std::string& send_uuid, TykeResponse& response,
                                                  uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        nonstd::expected<bool, std::string> SendAsync(const std::string& send_uuid,
                                                       uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        nonstd::expected<bool, std::string> SendAsyncWithFunc(const std::string& send_uuid,
                                                              const std::function<void(const TykeResponse &)> &func,
                                                              uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        nonstd::expected<ResponseFuture, std::string> SendAsyncWithFuture(const std::string& send_uuid,
                                                                           uint32_t timeout_ms = kIpcDefaultTimeoutMs);

    private:

        nonstd::expected<bool, std::string> EncodeAndSend(const std::string& send_uuid, MessageType msg_type,
                                                           uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        ProtocolHeader protocol_header_;
        RequestMetadata metadata_;
        std::vector<unsigned char> content_;

        inline static ObjectPool<TykeRequest> pool_;
    };

    
    struct TykeRequestDeleter
    {
        void operator()(TykeRequest* p) const
        {
            TykeRequest::Release(p);
        }
    };

    using TykeRequestPtr = std::unique_ptr<TykeRequest, TykeRequestDeleter>;

    
    inline TykeRequestPtr MakeRequestPtr()
    {
        return TykeRequestPtr(TykeRequest::Acquire());
    }
}
