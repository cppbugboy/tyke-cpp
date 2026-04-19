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
            {"module", t.module},
            {"async_uuid", t.async_uuid},
            {"msg_uuid", t.msg_uuid},
            {"route", t.route},
            {"content_type", t.content_type},
            {"timestamp", t.timestamp}
        };
    }

    void from_json(const nlohmann::json& j, RequestMetadata& t)
    {
        t.module = j.value("module", std::string{});
        t.async_uuid = j.value("async_uuid", std::string{});
        t.msg_uuid = j.value("msg_uuid", std::string{});
        t.route = j.value("route", std::string{});
        t.content_type = j.value("content_type", std::string{});
        t.timestamp = j.value("timestamp", std::string{});
    }

    const std::unordered_set<std::string>& RequestMetadata::JsonKeySet()
    {
        static const std::unordered_set<std::string> set = {
            "module", "async_uuid", "msg_uuid", "route", "content_type", "timestamp"
        };
        return set;
    }
}
