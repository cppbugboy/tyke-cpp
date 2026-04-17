/**
 * @file tyke_request.cpp
 * @brief Tyke请求对象实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现TykeRequest类的具体逻辑，包括对象池管理、请求发送等功能。
 */

#include "core/tyke_request.h"

#include "common/log_def.h"
#include "common/tyke_utils.h"
#include "core/data_proc.h"
#include "core/request_stub.h"
#include "ipc/ipc_client.h"

namespace tyke
{
    // 静态对象池实例
    ObjectPool<TykeRequest> TykeRequest::pool_;

    /**
     * @brief 重置请求对象状态
     */
    void TykeRequest::Reset()
    {
        protocol_header_ = ProtocolHeader{};
        metadata_ = RequestMetadata{};
        content_.clear();
        async_func_ = nullptr;
    }

    /**
     * @brief 从对象池获取请求对象
     * @return 请求对象指针
     */
    TykeRequest* TykeRequest::Acquire()
    {
        LOG_DEBUG("Acquiring request object from pool");
        return pool_.Acquire();
    }

    /**
     * @brief 将请求对象释放回对象池
     * @param req 请求对象指针
     */
    void TykeRequest::Release(TykeRequest* req)
    {
        if (req)
        {
            LOG_DEBUG("Releasing request object to pool, msg_uuid={}", req->GetMsgUuid());
            req->Reset();
            pool_.Release(req);
        }
    }

    /**
     * @brief 获取协议魔数
     * @return 4字节魔数字符串
     */
    const char* TykeRequest::GetMagic() const
    {
        return protocol_header_.magic;
    }

    /**
     * @brief 获取消息类型
     * @return 消息类型枚举值
     */
    MessageType TykeRequest::GetMessageType() const
    {
        return static_cast<MessageType>(protocol_header_.msg_type);
    }

    /**
     * @brief 获取请求内容
     * @param content_type 输出内容类型字符串
     * @param content 输出内容数据
     */
    void TykeRequest::GetContent(std::string& content_type, std::vector<unsigned char>& content) const
    {
        content_type = metadata_.GetContentType();
        content = content_;
    }

    /**
     * @brief 获取模块名称
     * @return 模块名称字符串
     */
    std::string TykeRequest::GetModule() const
    {
        return metadata_.GetModule();
    }

    /**
     * @brief 获取路由路径
     * @return 路由路径字符串
     */
    std::string TykeRequest::GetRoute() const
    {
        return metadata_.GetRoute();
    }

    /**
     * @brief 设置请求内容
     * @param content_type 内容类型
     * @param content 内容数据
     * @return 当前请求引用
     */
    TykeRequest& TykeRequest::SetContent(const ContentType& content_type, const std::vector<unsigned char>& content)
    {
        metadata_.SetContentType(ContentTypeMap().at(content_type));
        content_ = content;
        return *this;
    }

    /**
     * @brief 设置模块名称
     * @param module 模块名称
     * @return 当前请求引用
     */
    TykeRequest& TykeRequest::SetModule(const std::string& module)
    {
        metadata_.SetModule(module);
        return *this;
    }

    /**
     * @brief 设置路由路径
     * @param route 路由路径
     * @return 当前请求引用
     */
    TykeRequest& TykeRequest::SetRoute(const std::string& route)
    {
        metadata_.SetRoute(route);
        return *this;
    }

    /**
     * @brief 编码并发送请求
     * @param send_uuid 目标服务器UUID
     * @param msg_type 消息类型
     * @return 成功返回true，失败返回错误信息
     */
    nonstd::expected<bool, std::string> TykeRequest::EncodeAndSend(const std::string& send_uuid, MessageType msg_type)
    {
        LOG_DEBUG("EncodeAndSend: send_uuid={}, route={}, msg_type={}",
                  send_uuid, GetRoute(), static_cast<int>(msg_type));

        protocol_header_.msg_type = msg_type;
        metadata_.SetMsgUuid(utils::GenerateUUID()).SetTimestamp(utils::GenerateTimestamp());

        std::vector<unsigned char> data_vec;
        auto encode_result = DataProc::EncodeRequest(*this, data_vec);
        if (!encode_result)
        {
            LOG_ERROR("Encode request failed: {}", encode_result.error());
            return nonstd::make_unexpected("encode request failed: " + encode_result.error());
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

    /**
     * @brief 同步发送请求
     * @param send_uuid 目标服务器UUID
     * @param response 输出响应对象
     * @return 成功返回true，失败返回错误信息
     */
    nonstd::expected<bool, std::string> TykeRequest::Send(const std::string& send_uuid, TykeResponse& response)
    {
        LOG_DEBUG("Send: send_uuid={}, route={}", send_uuid, GetRoute());

        protocol_header_.msg_type = MessageType::kRequest;
        metadata_.SetMsgUuid(utils::GenerateUUID()).SetTimestamp(utils::GenerateTimestamp());

        std::vector<unsigned char> data_vec;
        auto encode_result = DataProc::EncodeRequest(*this, data_vec);
        if (!encode_result)
        {
            LOG_ERROR("Encode request failed: {}", encode_result.error());
            return nonstd::make_unexpected("encode request failed: " + encode_result.error());
        }

        auto send_result = IpcClient::Send(send_uuid, data_vec,
                                  [&response](const std::vector<unsigned char>& recv_data) -> bool
                                  {
                                      uint32_t data_size = 0;
                                      auto decode_result = DataProc::DecodeResponse(recv_data, response, data_size);
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

    /**
     * @brief 异步发送请求（无回调）
     * @param send_uuid 目标服务器UUID
     * @param recv_uuid 接收响应的服务器UUID（暂未使用）
     * @return 成功返回true，失败返回错误信息
     */
    nonstd::expected<bool, std::string> TykeRequest::SendAsync(const std::string& send_uuid,
                                                               const std::string&)
    {
        return EncodeAndSend(send_uuid, MessageType::kRequestAsync);
    }

    /**
     * @brief 异步发送请求（回调方式）
     * @param send_uuid 目标服务器UUID
     * @param func 响应回调函数
     * @return 成功返回true，失败返回错误信息
     */
    nonstd::expected<bool, std::string> TykeRequest::SendAsyncWithFunc(const std::string& send_uuid,
                                                                       std::function<void(TykeResponse &)> func)
    {
        LOG_DEBUG("SendAsyncWithFunc: send_uuid={}, route={}", send_uuid, GetRoute());

        auto result = EncodeAndSend(send_uuid, MessageType::kRequestAsyncFunc);
        if (result)
        {
            async_func_ = std::move(func);
            LOG_DEBUG("Async callback registered, msg_uuid={}", GetMsgUuid());
        }
        return result;
    }

    /**
     * @brief 异步发送请求（Future方式）
     * @param send_uuid 目标服务器UUID
     * @param recv_uuid 接收响应的服务器UUID（暂未使用）
     * @return 成功返回ResponseFuture对象，失败返回错误信息
     */
    nonstd::expected<ResponseFuture, std::string> TykeRequest::SendAsyncWithFuture(const std::string& send_uuid,
        const std::string&)
    {
        LOG_DEBUG("SendAsyncWithFuture: send_uuid={}, route={}", send_uuid, GetRoute());

        auto result = EncodeAndSend(send_uuid, MessageType::kResponseAsyncFunc);
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

    /**
     * @brief 添加自定义元数据
     * @param key 元数据键名
     * @param value 元数据值
     * @return 成功返回true，失败返回错误信息
     */
    nonstd::expected<bool, std::string> TykeRequest::AddMetadata(const std::string& key, const JsonValue& value)
    {
        return metadata_.AddMetadata(key, value);
    }

    /**
     * @brief 获取自定义元数据
     * @param key 元数据键名
     * @return 存在返回值，不存在返回nullopt
     */
    nonstd::optional<JsonValue> TykeRequest::GetMetadata(const std::string& key)
    {
        return metadata_.GetMetadata(key);
    }

    /**
     * @brief 获取消息UUID
     * @return 消息UUID字符串
     */
    std::string TykeRequest::GetMsgUuid() const
    {
        return metadata_.GetMsgUuid();
    }

    /**
     * @brief 获取异步UUID
     * @return 异步UUID字符串
     */
    std::string TykeRequest::GetAsyncUuid() const
    {
        return metadata_.GetAsyncUuid();
    }
} // tyke
