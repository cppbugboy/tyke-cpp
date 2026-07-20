/**
 * @file response_metadata.cpp
 * @brief 响应元数据实现。包含状态码/原因管理、JSON 序列化及 key 集合定义。
 * @author Nick
 * @date 2026/04/19
 */

#include "tyke/core/response_metadata.h"

namespace tyke
{
    StatusCode ResponseMetadata::GetStatus() const
    {
        return status_;
    }

    ResponseMetadata& ResponseMetadata::SetStatus(StatusCode status)
    {
        status_ = status;
        return *this;
    }

    const std::string& ResponseMetadata::GetReason() const
    {
        return reason_;
    }

    ResponseMetadata& ResponseMetadata::SetReason(std::string_view reason)
    {
        reason_ = reason;
        return *this;
    }

    /** @brief 将 ResponseMetadata 序列化为 JSON 对象（包含 status/reason 额外字段）。 */
    void to_json(nlohmann::json& j, const ResponseMetadata& t)
    {
        j = nlohmann::json{
            {"module", t.module_},
            {"async_uuid", t.async_uuid_},
            {"msg_uuid", t.msg_uuid_},
            {"route", t.route_},
            {"content_type", t.content_type_},
            {"timestamp", t.timestamp_},
            {"status", t.status_},
            {"reason", t.reason_},
            {"timeout", t.timeout_}
        };
    }

    /** @brief 从 JSON 对象反序列化为 ResponseMetadata。 */
    void from_json(const nlohmann::json& j, ResponseMetadata& t)
    {
        t.module_ = j.value("module", std::string{});
        t.async_uuid_ = j.value("async_uuid", std::string{});
        t.msg_uuid_ = j.value("msg_uuid", std::string{});
        t.route_ = j.value("route", std::string{});
        t.content_type_ = j.value("content_type", std::string{});
        t.timestamp_ = j.value("timestamp", std::string{});
        t.status_ = j.value("status", StatusCode::kNone);
        t.reason_ = j.value("reason", std::string{});
        t.timeout_ = j.value("timeout", uint64_t{});
    }

    /** @brief 返回 ResponseMetadata JSON 序列化涉及的 key 集合。 */
    const std::unordered_set<std::string>& ResponseMetadata::JsonKeySet()
    {
        static const std::unordered_set<std::string> set = {
            "module", "async_uuid", "msg_uuid", "route", "content_type",
            "timestamp", "timeout", "status", "reason"
        };
        return set;
    }
} // namespace tyke
