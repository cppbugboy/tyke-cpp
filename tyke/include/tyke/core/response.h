/**
 * @file response.h
 * @brief 响应对象。封装IPC响应的元数据、内容和发送功能。
 * @author Nick
 * @date 2026/04/19
 *
 * 对象生命周期通过对象池管理，调用 Acquire() 获取 shared_ptr，
 * 当引用计数归零时自动调用 Reset() 并归还到池中。
 */


#pragma once

#include <atomic>
#include <memory>
#include <string_view>

#include "tyke/common/tyke_def.h"
#include "tyke/component/object_pool.h"
#include "tyke/ipc/ipc_def.h"
#include "response_metadata.h"

namespace tyke
{
    /** @brief 发送数据回调函数类型：通过客户端ID发送字节流。 */
    using SendDataHandler = std::function<bool(ClientId, const std::vector<uint8_t>&)>;

    /** @brief 响应状态追踪，原子标记是否已发送。 */
    struct ResponseState
    {
        std::atomic<bool> is_send{false};
    };

    /** @brief 响应对象。封装IPC响应的元数据、内容和发送功能。 */
    class Response
    {
        friend class DataProc;
        friend class RequestStub;

    public:
        // 使用 shared_ptr 配合自定义删除器实现池化回收
        using Ptr = std::shared_ptr<Response>;

        /** @brief 从对象池中获取一个响应对象。
         * @return Ptr shared_ptr，当无引用时自动 Reset 并归还池。
         */
        static Ptr Acquire()
        {
            auto& pool = GetPool();
            return Ptr(pool.Acquire(),
                       [&pool](Response* p)
                       {
                           if (p)
                           {
                               p->Reset();
                               pool.Release(p);
                           }
                       });
        }

        /** @brief 重置所有成员到默认状态。 */
        void Reset();

        Response() = default;

        Response(const Response&) = delete;
        Response& operator=(const Response&) = delete;
        Response(Response&&) = default;
        Response& operator=(Response&&) = default;

        /** @brief 获取协议魔数。 */
        [[nodiscard]] const char* GetMagic() const;

        /** @brief 设置消息类型。 */
        Response& SetMessageType(MessageType msg_type);
        /** @brief 获取消息类型。 */
        [[nodiscard]] MessageType GetMessageType() const;

        /** @brief 设置模块名。 */
        Response& SetModule(std::string_view module);
        /** @brief 获取模块名。 */
        [[nodiscard]] const std::string& GetModule() const;

        /** @brief 设置消息UUID。 */
        Response& SetMsgUuid(std::string_view msg_uuid);
        /** @brief 获取消息UUID。 */
        [[nodiscard]] const std::string& GetMsgUuid() const;

        /** @brief 设置路由路径。 */
        Response& SetRoute(std::string_view route);
        /** @brief 获取路由路径。 */
        [[nodiscard]] const std::string& GetRoute() const;

        /** @brief 获取二进制内容。 */
        void GetContent(std::string& content_type, std::vector<uint8_t>& content) const;
        /** @brief 设置二进制内容。 */
        Response& SetContent(const ContentType& content_type, const std::vector<uint8_t>& response_content);

        /** @brief 设置字符串内容。 */
        Response& SetContent(const ContentType& content_type, const std::string& content);
        /** @brief 获取字符串内容。 */
        void GetContent(std::string& content_type, std::string& content) const;

        /** @brief 添加自定义元数据键值对。 */
        std::optional<bool> AddMetadata(std::string_view key, const JsonValue& value);
        /** @brief 获取自定义元数据值。 */
        [[nodiscard]] std::optional<JsonValue> GetMetadata(std::string_view key) const;

        /** @brief 设置响应状态码和原因。 */
        Response& SetResult(StatusCode status, std::string_view reason);
        /** @brief 获取响应状态码和原因。 */
        void GetResult(StatusCode& status, std::string& reason) const;

        /** @brief 设置异步回调目标UUID。 */
        Response& SetAsyncUuid(std::string_view target_uuid);
        /** @brief 获取异步回调目标UUID。 */
        [[nodiscard]] const std::string& GetAsyncUuid() const;

        /** @brief 设置发送数据回调。 */
        Response& SetSendDataHandler(const SendDataHandler& send_data_handler);
        /** @brief 设置目标客户端ID。 */
        Response& SetClientId(ClientId client_id);

        /** @brief 同步发送响应。 */
        [[nodiscard]] BoolResult Send();
        /** @brief 异步发送响应。 */
        [[nodiscard]] BoolResult SendAsync();
        /** @brief 检查响应是否已发送。 */
        [[nodiscard]] bool IsSent() const;

    private:
        std::shared_ptr<ResponseState> state_ = std::make_shared<ResponseState>();
        ProtocolHeader protocol_header_;
        ResponseMetadata metadata_;
        std::vector<uint8_t> content_;
        ClientId client_id_{};
        SendDataHandler send_data_handler_;

        static ObjectPool<Response>& GetPool()
        {
            static ObjectPool<Response> pool{1024};
            return pool;
        }
    };
} // namespace tyke
