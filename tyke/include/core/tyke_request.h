/**
 * @file tyke_request.h
 * @brief 请求对象声明。封装IPC请求的元数据、内容和发送功能，支持同步和异步发送。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <future>
#include <string_view>

#include "request_metadata.h"
#include "tyke_response.h"
#include "response_future.h"
#include "common/tyke_def.h"
#include "ipc/ipc_def.h"
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

        
        [[nodiscard]] const char* GetMagic() const;

        
        [[nodiscard]] MessageType GetMessageType() const;

        
        TykeRequest& SetContent(const ContentType& content_type, const std::vector<unsigned char>& content);


        void GetContent(std::string& content_type, std::vector<unsigned char>& content) const;


        TykeRequest& SetModule(std::string_view module);


        [[nodiscard]] const std::string& GetModule() const;


        TykeRequest& SetRoute(std::string_view route);


        [[nodiscard]] const std::string& GetRoute() const;

        
        [[nodiscard]] const std::string& GetMsgUuid() const;

        TykeRequest& SetAsyncUuid(std::string_view async_uuid);
        
        [[nodiscard]] const std::string& GetAsyncUuid() const;

        
        std::optional<bool> AddMetadata(std::string_view key, const JsonValue& value);


        [[nodiscard]] std::optional<JsonValue> GetMetadata(std::string_view key) const;

        
        BoolResult Send(const std::string& send_uuid, TykeResponse& response,
                        uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        BoolResult SendAsync(const std::string& send_uuid,
                             uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        BoolResult SendAsyncWithFunc(const std::string& send_uuid,
                                     const std::function<void(const TykeResponse &)> &func,
                                     uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        nonstd::expected<ResponseFuture, std::string> SendAsyncWithFuture(const std::string& send_uuid,
                                                                           uint32_t timeout_ms = kIpcDefaultTimeoutMs);

    private:

        BoolResult EncodeAndSend(const std::string& send_uuid, MessageType msg_type,
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
