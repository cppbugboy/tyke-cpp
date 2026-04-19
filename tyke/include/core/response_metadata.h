/**
 * @file response_metadata.h
 * @brief 响应元数据声明。继承MetadataBase，额外包含状态码和原因描述。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <nlohmann/json.hpp>

#include "core/metadata_base.h"

namespace tyke
{

    class ResponseMetadata : public MetadataBase<ResponseMetadata>
    {
    public:
        int GetStatus() const { return status; }
        ResponseMetadata& SetStatus(int in_status)
        {
            status = in_status;
            return *this;
        }

        const std::string& GetReason() const { return reason; }
        ResponseMetadata& SetReason(std::string_view in_reason)
        {
            reason = in_reason;
            return *this;
        }

        friend void to_json(nlohmann::json& j, const ResponseMetadata& t)
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

        friend void from_json(const nlohmann::json& j, ResponseMetadata& t)
        {
            t.module = j.value("module", std::string{});
            t.msg_uuid = j.value("msg_uuid", std::string{});
            t.route = j.value("route", std::string{});
            t.content_type = j.value("content_type", std::string{});
            t.timestamp = j.value("timestamp", std::string{});
            t.status = j.value("status", 0);
            t.reason = j.value("reason", std::string{});
        }

        static const std::unordered_set<std::string>& JsonKeySet()
        {
            static const std::unordered_set<std::string> set = {
                "module", "msg_uuid", "route", "content_type", "timestamp", "status", "reason"
            };
            return set;
        }

    private:
        int status = 0;
        std::string reason;
    };
}
