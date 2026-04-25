/**
 * @file request_metadata.h
 * @brief 请求元数据声明。继承MetadataBase，存储请求的模块名、路由、UUID等元信息。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

#include "core/metadata_base.h"

namespace tyke
{
    class RequestMetadata : public MetadataBase<RequestMetadata>
    {
    public:
        friend void to_json(nlohmann::json& j, const RequestMetadata& t);
        friend void from_json(const nlohmann::json& j, RequestMetadata& t);
        static const std::unordered_set<std::string>& JsonKeySet();
    };
} // namespace tyke