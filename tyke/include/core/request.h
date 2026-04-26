/**
 * @file tyke_request.h
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

#include "common/tyke_def.h"
#include "component/object_pool.h"
#include "ipc/ipc_def.h"
#include "request_metadata.h"
#include "response.h"

namespace tyke
{
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

        [[nodiscard]] const char* GetMagic() const;
        [[nodiscard]] MessageType GetMessageType() const;

        Request& SetContent(const ContentType& content_type, const std::vector<uint8_t>& content);
        void GetContent(std::string& content_type, std::vector<uint8_t>& content) const;

        Request& SetModule(std::string_view module);
        [[nodiscard]] const std::string& GetModule() const;

        Request& SetRoute(std::string_view route);
        [[nodiscard]] const std::string& GetRoute() const;

        [[nodiscard]] const std::string& GetMsgUuid() const;

        Request& SetAsyncUuid(std::string_view async_uuid);
        [[nodiscard]] const std::string& GetAsyncUuid() const;

        Request& SetTimeout(uint64_t timeout);
        [[nodiscard]] uint64_t GetTimeout() const;

        std::optional<bool> AddMetadata(std::string_view key, const JsonValue& value);
        [[nodiscard]] std::optional<JsonValue> GetMetadata(std::string_view key) const;

        [[nodiscard]] BoolResult Send(const std::string& send_uuid, Response& response,
                                      uint32_t timeout_ms = kIpcDefaultTimeoutMs);
        [[nodiscard]] BoolResult SendAsync(const std::string& send_uuid, uint32_t timeout_ms = kIpcDefaultTimeoutMs);
        [[nodiscard]] BoolResult SendAsyncWithFunc(const std::string& send_uuid,
                                                   const std::function<void(const Response &)>& func,
                                                   uint32_t timeout_ms = kIpcDefaultTimeoutMs);
        [[nodiscard]] nonstd::expected<std::future<Response>, std::string>
        SendAsyncWithFuture(const std::string& send_uuid, uint32_t timeout_ms = kIpcDefaultTimeoutMs);

    private:
        BoolResult EncodeAndSend(const std::string& send_uuid, MessageType msg_type,
                                 uint32_t timeout_ms = kIpcDefaultTimeoutMs);

        ProtocolHeader protocol_header_;
        RequestMetadata metadata_;
        std::vector<uint8_t> content_;

        static ObjectPool<Request>& GetPool()
        {
            static ObjectPool<Request> pool{1024};
            return pool;
        }
    };
} // namespace tyke