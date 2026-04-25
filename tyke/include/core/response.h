/**
 * @file tyke_response.h
 * @brief 响应对象。封装IPC响应的元数据、内容和发送功能。
 * @author Nick
 * @date 2026/04/19
 *
 * 对象生命周期通过对象池管理，调用 Acquire() 获取 shared_ptr，
 * 当引用计数归零时自动调用 Reset() 并归还到池中。
 */

#pragma once

#include <memory>
#include <string_view>

#include "common/tyke_def.h"
#include "component/object_pool.h"
#include "ipc/ipc_def.h"
#include "response_metadata.h"

namespace tyke
{
using SendDataHandler = std::function<bool(ClientId, const std::vector<uint8_t> &)>;

class Response
{
    friend class DataProc;
    friend class RequestStub;

public:
    // 使用 shared_ptr 配合自定义删除器实现池化回收
    using Ptr = std::shared_ptr<Response>;

    /**
         * @brief 从对象池中获取一个响应对象。
         * @return Ptr shared_ptr，当无引用时自动 Reset 并归还池。
         */
    static Ptr Acquire()
    {
        return Ptr(pool_.Acquire(),
                   [](Response *p)
                   {
                       p->Reset();
                       pool_.Release(p);
                   });
    }

    /** @brief 重置所有成员到默认状态。 */
    void Reset();

    Response();

    [[nodiscard]] const char *GetMagic() const;

    Response                 &SetMessageType(MessageType msg_type);
    [[nodiscard]] MessageType GetMessageType() const;

    Response                        &SetModule(std::string_view module);
    [[nodiscard]] const std::string &GetModule() const;

    Response                        &SetMsgUuid(std::string_view msg_uuid);
    [[nodiscard]] const std::string &GetMsgUuid() const;

    Response                        &SetRoute(std::string_view route);
    [[nodiscard]] const std::string &GetRoute() const;

    void      GetContent(std::string &content_type, std::vector<uint8_t> &content) const;
    Response &SetContent(const ContentType &content_type, const std::vector<uint8_t> &response_content);

    std::optional<bool>                    AddMetadata(std::string_view key, const JsonValue &value);
    [[nodiscard]] std::optional<JsonValue> GetMetadata(std::string_view key) const;

    Response &SetResult(StatusCode status, std::string_view reason);
    void      GetResult(StatusCode &status, std::string &reason) const;

    Response                        &SetAsyncUuid(std::string_view target_uuid);
    [[nodiscard]] const std::string &GetAsyncUuid() const;

    Response &SetSendDataHandler(const SendDataHandler &send_data_handler);
    Response &SetClientId(ClientId client_id);

    [[nodiscard]] BoolResult Send();
    [[nodiscard]] BoolResult SendAsync();

private:
    ProtocolHeader       protocol_header_;
    ResponseMetadata     metadata_;
    std::vector<uint8_t> content_;
    bool                 is_send_ = false;
    ClientId             client_id_{};
    SendDataHandler      send_data_handler_;

    inline static ObjectPool<Response> pool_{1024};// 全局共享对象池
};
}// namespace tyke
