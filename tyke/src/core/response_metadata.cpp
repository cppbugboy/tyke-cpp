/**
 * @file response_metadata.cpp
 * @brief 响应元数据实现。
 * @author Nick
 * @date 2026/04/19
 */

#include "core/response_metadata.h"

namespace tyke
{
    int ResponseMetadata::GetStatus() const
    {
        return status;
    }

    ResponseMetadata& ResponseMetadata::SetStatus(int in_status)
    {
        status = in_status;
        return *this;
    }

    const std::string& ResponseMetadata::GetReason() const
    {
        return reason;
    }

    ResponseMetadata& ResponseMetadata::SetReason(std::string_view in_reason)
    {
        reason = in_reason;
        return *this;
    }

    void to_json(nlohmann::json& j, const ResponseMetadata& t)
    {
        j = nlohmann::json{
            {"module", t.module},
            {"msg_uuid", t.msg_uuid},
            {"route", t.route},
            {"content_type", t.content_type},
            {"timestamp", t.timestamp},
            {"status", t.status},
            {"reason", t.reason}
        };
    }

    void from_json(const nlohmann::json& j, ResponseMetadata& t)
    {
        t.module = j.value("module", std::string{});
        t.msg_uuid = j.value("msg_uuid", std::string{});
        t.route = j.value("route", std::string{});
        t.content_type = j.value("content_type", std::string{});
        t.timestamp = j.value("timestamp", std::string{});
        t.status = j.value("status", 0);
        t.reason = j.value("reason", std::string{});
    }

    const std::unordered_set<std::string>& ResponseMetadata::JsonKeySet()
    {
        static const std::unordered_set<std::string> set = {
            "module", "msg_uuid", "route", "content_type", "timestamp", "status", "reason"
        };
        return set;
    }
}
