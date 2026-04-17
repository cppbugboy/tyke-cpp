/**
 * @file tyke_response.cpp
 * @brief Tyke响应对象实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现TykeResponse类的具体逻辑，包括对象池管理、响应发送等功能。
 */

#include "core/tyke_response.h"

#include "common/log_def.h"
#include "common/tyke_utils.h"
#include "core/data_proc.h"
#include "ipc/ipc_client.h"

namespace tyke
{
    // 静态对象池实例
    ObjectPool<TykeResponse> TykeResponse::pool_;

    TykeResponse::TykeResponse() = default;

    /**
     * @brief 重置响应对象状态
     */
    void TykeResponse::Reset()
    {
        protocol_header_ = ProtocolHeader{};
        metadata_ = ResponseMetadata{};
        content_.clear();
        data_size_ = 0;
        is_send_ = false;
        client_id_ = ClientId{};
        send_data_handler_ = nullptr;
        target_uuid_.clear();
    }

    /**
     * @brief 从对象池获取响应对象
     * @return 响应对象指针
     */
    TykeResponse* TykeResponse::Acquire()
    {
        LOG_DEBUG("Acquiring response object from pool");
        return pool_.Acquire();
    }

    /**
     * @brief 将响应对象释放回对象池
     * @param resp 响应对象指针
     */
    void TykeResponse::Release(TykeResponse* resp)
    {
        if (resp)
        {
            LOG_DEBUG("Releasing response object to pool, msg_uuid={}", resp->GetMsgUuid());
            resp->Reset();
            pool_.Release(resp);
        }
    }

    /**
     * @brief 获取协议魔数
     * @return 4字节魔数字符串
     */
    const char* TykeResponse::GetMagic() const
    {
        return protocol_header_.magic;
    }

    /**
     * @brief 获取消息UUID
     * @return 消息UUID字符串
     */
    std::string TykeResponse::GetMsgUuid() const
    {
        return metadata_.GetMsgUuid();
    }

    /**
     * @brief 设置路由路径
     * @param route 路由路径
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetRoute(const std::string& route)
    {
        metadata_.SetRoute(route);
        return *this;
    }

    /**
     * @brief 获取路由路径
     * @return 路由路径字符串
     */
    std::string TykeResponse::GetRoute() const
    {
        return metadata_.GetRoute();
    }

    /**
     * @brief 设置响应内容
     * @param content_type 内容类型
     * @param response_content 内容数据
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetContent(const ContentType& content_type,
                                           const std::vector<unsigned char>& response_content)
    {
        metadata_.SetContentType(ContentTypeMap().at(content_type));
        content_ = response_content;
        return *this;
    }

    /**
     * @brief 设置消息类型
     * @param msg_type 消息类型枚举值
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetMessageType(const MessageType msg_type)
    {
        protocol_header_.msg_type = msg_type;
        return *this;
    }

    /**
     * @brief 获取消息类型
     * @return 消息类型枚举值
     */
    MessageType TykeResponse::GetMessageType() const
    {
        return static_cast<MessageType>(protocol_header_.msg_type);
    }

    /**
     * @brief 设置模块名称
     * @param module 模块名称
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetModule(const std::string& module)
    {
        metadata_.SetModule(module);
        return *this;
    }

    /**
     * @brief 获取模块名称
     * @return 模块名称字符串
     */
    std::string TykeResponse::GetModule() const
    {
        return metadata_.GetModule();
    }

    /**
     * @brief 设置消息UUID
     * @param msg_uuid 消息UUID
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetMsgUuid(const std::string& msg_uuid)
    {
        metadata_.SetMsgUuid(msg_uuid);
        return *this;
    }

    /**
     * @brief 获取响应内容
     * @param content_type 输出内容类型字符串
     * @param content 输出内容数据
     */
    void TykeResponse::GetContent(std::string& content_type, std::vector<unsigned char>& content) const
    {
        content_type = metadata_.GetContentType();
        content = content_;
    }

    /**
     * @brief 添加自定义元数据
     * @param key 元数据键名
     * @param value 元数据值
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult TykeResponse::AddMetadata(const std::string& key, const JsonValue& value)
    {
        return metadata_.AddMetadata(key, value);
    }

    /**
     * @brief 获取自定义元数据
     * @param key 元数据键名
     * @return 存在返回值，不存在返回nullopt
     */
    nonstd::optional<JsonValue> TykeResponse::GetMetadata(const std::string& key)
    {
        return metadata_.GetMetadata(key);
    }

    /**
     * @brief 设置响应结果
     * @param status 状态码
     * @param reason 原因描述
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetResult(const int status, const std::string& reason)
    {
        metadata_.SetStatus(status).SetReason(reason);
        return *this;
    }

    /**
     * @brief 获取响应结果
     * @param status 输出状态码
     * @param reason 输出原因描述
     */
    void TykeResponse::GetResult(int& status, std::string& reason) const
    {
        status = metadata_.GetStatus();
        reason = metadata_.GetReason();
    }

    /**
     * @brief 同步发送响应
     * @return 成功返回true，失败返回错误信息
     */
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
    std::vector<unsigned char> data_vec;
    auto encode_result = DataProc::EncodeResponse(*this, data_vec);
    if (!encode_result)
    {
            LOG_ERROR("Encode response failed: {}", encode_result.error());
        return nonstd::make_unexpected("encode response failed: " + encode_result.error());
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

    /**
     * @brief 异步发送响应
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult TykeResponse::SendAsync()
    {
        LOG_DEBUG("SendAsync: route={}, msg_uuid={}, target_uuid={}", GetRoute(), GetMsgUuid(), target_uuid_);

        if (is_send_)
        {
            LOG_WARN("Response already sent, msg_uuid={}", GetMsgUuid());
            return nonstd::make_unexpected("response already sent");
        }

        metadata_.SetTimestamp(utils::GenerateTimestamp());
        std::vector<unsigned char> data_vec;
        auto encode_result = DataProc::EncodeResponse(*this, data_vec);
        if (!encode_result)
        {
            LOG_ERROR("Encode response failed: {}", encode_result.error());
            return nonstd::make_unexpected("encode response failed: " + encode_result.error());
        }

        auto send_result = IpcClient::SendAsync(target_uuid_, data_vec);
        if (!send_result)
        {
            LOG_ERROR("Send async failed: {}", send_result.error());
            return nonstd::make_unexpected("send async failed: " + send_result.error());
        }

        is_send_ = true;
        LOG_DEBUG("Async response sent successfully, msg_uuid={}", GetMsgUuid());
        return true;
    }

    /**
     * @brief 设置异步目标UUID
     * @param target_uuid 目标服务器UUID
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetAsyncUuid(const std::string& target_uuid)
    {
        target_uuid_ = target_uuid;
        return *this;
    }

    /**
     * @brief 获取异步目标UUID
     * @return 目标服务器UUID字符串
     */
    std::string TykeResponse::GetAsyncUuid() const
    {
        return target_uuid_;
    }

    /**
     * @brief 设置发送数据回调函数
     * @param send_data_handler 发送数据的回调函数
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetSendDataHandler(const SendDataHandler& send_data_handler)
    {
        send_data_handler_ = send_data_handler;
        return *this;
    }

    /**
     * @brief 设置IPC客户端标识
     * @param client_id 客户端标识
     * @return 当前响应引用
     */
    TykeResponse& TykeResponse::SetIpcFD(const ClientId client_id)
    {
        client_id_ = client_id;
        return *this;
    }
} // tyke
