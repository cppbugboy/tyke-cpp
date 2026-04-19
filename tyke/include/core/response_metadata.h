/**
 * @file response_metadata.h
 * @brief 响应元数据声明。继承MetadataBase，额外包含状态码和原因描述。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef TYKE_RESPONSE_META_DATA_H
#define TYKE_RESPONSE_META_DATA_H
#include <string>
#include <unordered_set>
#include <nlohmann/json.hpp>

#include "common/tyke_result.h"
#include "common/common_def.h"
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
        ResponseMetadata& SetReason(const std::string& in_reason)
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
            j.at("module").get_to(t.module);
            j.at("msg_uuid").get_to(t.msg_uuid);
            j.at("route").get_to(t.route);
            j.at("content_type").get_to(t.content_type);
            j.at("timestamp").get_to(t.timestamp);
            j.at("status").get_to(t.status);
            j.at("reason").get_to(t.reason);
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

#endif
