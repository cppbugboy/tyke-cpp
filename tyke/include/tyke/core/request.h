/**
 * @file request.h
 * @brief 请求对象。封装IPC请求的元数据、内容和发送功能，支持同步/异步发送。
 * @author Nick
 * @date 2026/04/19
 *
 * 对象生命周期通过对象池管理，调用 Acquire() 获取 shared_ptr，
 * 当引用计数归零时自动调用 Reset() 并归还到池中。
 */

#pragma once

#include <future>
#include <memory>
#include <string_view>

#include "tyke/common/tyke_def.h"
#include "tyke/component/object_pool.h"
#include "tyke/ipc/ipc_def.h"
#include "request_metadata.h"
#include "response.h"
#include "tyke/component/context.h"

namespace tyke
{
    /** @brief 请求对象。封装IPC请求的元数据、内容和发送功能，支持同步/异步发送。 */
    class Request
    {
        friend class DataProc;

    public:
        // 使用 shared_ptr 配合自定义删除器实现池化回收
        using Ptr = std::shared_ptr<Request>;

        /**
         * @brief 从对象池中获取一个请求对象。
         * @return Ptr shared_ptr，当无引用时自动 Reset 并归还池。
         */
        static Ptr Acquire()
        {
            auto& pool = GetPool();
            return Ptr(pool.Acquire(),
                       [&pool](Request* p)
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

        /** @brief 获取协议魔数。 */
        [[nodiscard]] const char* GetMagic() const;
        /** @brief 获取消息类型。 */
        [[nodiscard]] MessageType GetMessageType() const;

        /** @brief 设置二进制内容。 */
        Request& SetContent(const ContentType& content_type, const std::vector<uint8_t>& content);
        /** @brief 获取二进制内容。 */
        void GetContent(std::string& content_type, std::vector<uint8_t>& content) const;

        /** @brief 设置字符串内容。 */
        Request& SetContent(const ContentType& content_type, const std::string& content);
        /** @brief 获取字符串内容。 */
        void GetContent(std::string& content_type, std::string& content) const;

        /** @brief 设置目标模块名。 */
        Request& SetModule(std::string_view module);
        /** @brief 获取目标模块名。 */
        [[nodiscard]] const std::string& GetModule() const;

        /** @brief 设置目标路由路径。 */
        Request& SetRoute(std::string_view route);
        /** @brief 获取目标路由路径。 */
        [[nodiscard]] const std::string& GetRoute() const;

        /** @brief 获取消息UUID。 */
        [[nodiscard]] const std::string& GetMsgUuid() const;

        /** @brief 设置异步回调UUID。 */
        Request& SetAsyncUuid(std::string_view async_uuid);
        /** @brief 获取异步回调UUID。 */
        [[nodiscard]] const std::string& GetAsyncUuid() const;

        /** @brief 设置超时时间(毫秒)。 */
        Request& SetTimeout(uint64_t timeout);
        /** @brief 获取超时时间(毫秒)。 */
        [[nodiscard]] uint64_t GetTimeout() const;

        /** @brief 添加自定义元数据键值对。 */
        std::optional<bool> AddMetadata(std::string_view key, const JsonValue& value);
        /** @brief 获取自定义元数据值。 */
        [[nodiscard]] std::optional<JsonValue> GetMetadata(std::string_view key) const;

        /** @brief 同步发送请求，阻塞等待响应。 */
        [[nodiscard]] BoolResult Send(const std::string& send_uuid, Response& response,
                                      uint32_t timeout_ms = kIpcDefaultTimeoutMs);
        /** @brief 异步发送请求，不等待响应。 */
        [[nodiscard]] BoolResult SendAsync(const std::string& send_uuid, uint32_t timeout_ms = kIpcDefaultTimeoutMs);
        /** @brief 异步发送请求，通过回调函数处理响应。 */
        [[nodiscard]] BoolResult SendAsyncWithFunc(const std::string& send_uuid,
                                                   const std::function<void(const Response &)>& func,
                                                   uint32_t timeout_ms = kIpcDefaultTimeoutMs);
        /** @brief 异步发送请求，返回future用于等待响应。 */
        [[nodiscard]] nonstd::expected<std::future<Response>, std::string>
        SendAsyncWithFuture(const std::string& send_uuid, uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        /** @brief 设置请求上下文。 */
        Request& SetContext(const std::shared_ptr<Context>& context);
        /** @brief 获取请求上下文。 */
        [[nodiscard]] std::shared_ptr<Context> GetContext() const;

    private:
        /** @brief 编码并发送请求数据包。 */
        BoolResult EncodeAndSend(const std::string& send_uuid, MessageType msg_type,
                                 uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        ProtocolHeader protocol_header_;
        RequestMetadata metadata_;
        std::vector<uint8_t> content_;
        std::shared_ptr<Context> context_;

        static ObjectPool<Request>& GetPool()
        {
            static ObjectPool<Request> pool{1024};
            return pool;
        }
    };
} // namespace tyke