/**
 * @file tyke_response.h
 * @brief 响应对象声明。封装IPC响应的元数据、内容和发送功能。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef TYKE_RESPONSE_H
#define TYKE_RESPONSE_H

#include "response_metadata.h"
#include "common/tyke_def.h"
#include "common/tyke_result.h"
#include "ipc/ipc_types.h"
#include "component/object_pool.h"

namespace tyke
{

    using SendDataHandler = std::function<bool(ClientId, const std::vector<unsigned char> &)>;

    
    class TykeResponse
    {
        friend class DataProc;
        friend class RequestStub;

    public:
        
        static TykeResponse* Acquire();

        
        static void Release(TykeResponse* resp);

        
        void Reset();

        TykeResponse();

        
        const char* GetMagic() const;

        
        TykeResponse& SetMessageType(MessageType msg_type);

        
        MessageType GetMessageType() const;

        
        TykeResponse& SetModule(const std::string& module);

        
        const std::string& GetModule() const;

        
        TykeResponse& SetMsgUuid(const std::string& msg_uuid);

        
        const std::string& GetMsgUuid() const;

        
        TykeResponse& SetRoute(const std::string& route);

        
        const std::string& GetRoute() const;

        
        void GetContent(std::string& content_type, std::vector<unsigned char>& content) const;

        
        TykeResponse& SetContent(const ContentType& content_type,
                                 const std::vector<unsigned char>& response_content);

        
        nonstd::expected<bool, std::string> AddMetadata(const std::string& key, const JsonValue& value);

        
        nonstd::optional<JsonValue> GetMetadata(const std::string& key);

        
        TykeResponse& SetResult(int status, const std::string& reason);

        
        void GetResult(int& status, std::string& reason) const;

        
        TykeResponse& SetAsyncUuid(const std::string& target_uuid);

        
        const std::string& GetAsyncUuid() const;

        
        TykeResponse& SetSendDataHandler(const SendDataHandler& send_data_handler);

        
        TykeResponse& SetClientId(ClientId client_id);

        
        BoolResult Send();

        
        BoolResult SendAsync();

    private:
        ProtocolHeader protocol_header_;
        ResponseMetadata metadata_;
        std::vector<unsigned char> content_;
        bool is_send_ = false;
        ClientId client_id_{};
        SendDataHandler send_data_handler_;
        std::string target_uuid_;

        static ObjectPool<TykeResponse> pool_;
    };

    
    struct TykeResponseDeleter
    {
        void operator()(TykeResponse* p) const
        {
            TykeResponse::Release(p);
        }
    };

    using TykeResponsePtr = std::unique_ptr<TykeResponse, TykeResponseDeleter>;

    
    inline TykeResponsePtr MakeResponsePtr()
    {
        return TykeResponsePtr(TykeResponse::Acquire());
    }
}

#endif