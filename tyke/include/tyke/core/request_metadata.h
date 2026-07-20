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

#include "tyke/core/metadata_base.h"

namespace tyke
{
    /** @brief 请求元数据。继承 MetadataBase，存储请求专属的模块、路由、UUID 等元信息。
     * @note 通过 CRTP 模式共享 MetadataBase 的通用字段和方法。
     */
    class RequestMetadata : public MetadataBase<RequestMetadata>
    {
    public:
        friend void to_json(nlohmann::json& j, const RequestMetadata& t);
        friend void from_json(const nlohmann::json& j, RequestMetadata& t);

        /** @brief 返回已注册的 JSON 键名集合，用于区分已知字段和自定义头部。 */
        static const std::unordered_set<std::string>& JsonKeySet();
    };
} // namespace tyke