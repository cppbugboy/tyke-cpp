/**
 * @file request_metadata.cpp
 * @brief 请求元数据实现。
 * @author Nick
 * @date 2026/04/19
 */

#include "core/request_metadata.h"

namespace tyke
{
    void to_json(nlohmann::json& j, const RequestMetadata& t)
    {
        j = nlohmann::json{
            {"module", t.module_}, {"async_uuid", t.async_uuid_}, {"msg_uuid", t.msg_uuid_},
            {"route", t.route_}, {"content_type", t.content_type_}, {"timestamp", t.timestamp_},
            {"timeout", t.timeout_}
        };
    }

    void from_json(const nlohmann::json& j, RequestMetadata& t)
    {
        t.module_ = j.value("module", std::string{});
        t.async_uuid_ = j.value("async_uuid", std::string{});
        t.msg_uuid_ = j.value("msg_uuid", std::string{});
        t.route_ = j.value("route", std::string{});
        t.content_type_ = j.value("content_type", std::string{});
        t.timestamp_ = j.value("timestamp", std::string{});
        t.timeout_ = j.value("timeout", uint64_t{});
    }

    const std::unordered_set<std::string>& RequestMetadata::JsonKeySet()
    {
        static const std::unordered_set<std::string> set = {
            "module", "async_uuid", "msg_uuid", "route",
            "content_type", "timeout", "timestamp"
        };
        return set;
    }
} // namespace tyke