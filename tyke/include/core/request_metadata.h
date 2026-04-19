/**
 * @file request_metadata.h
 * @brief 请求元数据声明。继承MetadataBase，存储请求的模块名、路由、UUID等元信息。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef TYKE_REQUEST_META_DATA_H
#define TYKE_REQUEST_META_DATA_H
#include <string>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include "common/tyke_result.h"
#include "common/common_def.h"
#include "core/metadata_base.h"

namespace tyke
{
    
    class RequestMetadata : public MetadataBase<RequestMetadata>
    {
    public:
        friend void to_json(nlohmann::json& j, const RequestMetadata& t)
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

        friend void from_json(const nlohmann::json& j, RequestMetadata& t)
        {
            j.at("module").get_to(t.module);
            j.at("async_uuid").get_to(t.async_uuid);
            j.at("msg_uuid").get_to(t.msg_uuid);
            j.at("route").get_to(t.route);
            j.at("content_type").get_to(t.content_type);
            j.at("timestamp").get_to(t.timestamp);
        }

        static const std::unordered_set<std::string>& JsonKeySet()
        {
            static const std::unordered_set<std::string> set = {
                "module", "async_uuid", "msg_uuid", "route", "content_type", "timestamp"
            };
            return set;
        }
    };
}

#endif
