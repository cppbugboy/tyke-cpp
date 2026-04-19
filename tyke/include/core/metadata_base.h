/**
 * @file metadata_base.h
 * @brief 元数据模板基类。使用CRTP模式提取请求/响应元数据的公共字段和方法。
 * @author Nick
 * @date 2026/04/19
 */

#ifndef TYKE_METADATA_BASE_H
#define TYKE_METADATA_BASE_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "common/tyke_result.h"
#include "common/common_def.h"

namespace tyke
{
    template<typename Derived>
    class MetadataBase
    {
    public:
        const std::string& GetModule() const
        {
            return module;
        }

        Derived& SetModule(const std::string& in_module)
        {
            module = in_module;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetAsyncUuid() const
        {
            return async_uuid;
        }

        Derived& SetAsyncUuid(const std::string& in_async_uuid)
        {
            async_uuid = in_async_uuid;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetMsgUuid() const
        {
            return msg_uuid;
        }

        Derived& SetMsgUuid(const std::string& in_msg_uuid)
        {
            msg_uuid = in_msg_uuid;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetRoute() const
        {
            return route;
        }

        Derived& SetRoute(const std::string& in_route)
        {
            route = in_route;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetContentType() const
        {
            return content_type;
        }

        Derived& SetContentType(const std::string& in_content_type)
        {
            content_type = in_content_type;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetTimestamp() const
        {
            return timestamp;
        }

        Derived& SetTimestamp(const std::string& in_timestamp)
        {
            timestamp = in_timestamp;
            return static_cast<Derived&>(*this);
        }

        nonstd::expected<bool, std::string> AddMetadata(const std::string& key, const JsonValue& value)
        {
            if (key.empty())
            {
                return nonstd::make_unexpected("Metadata key cannot be empty");
            }
            try
            {
                headers_map_[key] = value;
            }
            catch (const std::exception& e)
            {
                return nonstd::make_unexpected(std::string("Failed to add metadata: ") + e.what());
            }
            return true;
        }

        nonstd::optional<JsonValue> GetMetadata(const std::string& key) const
        {
            auto it = headers_map_.find(key);
            if (it != headers_map_.end())
            {
                return it->second;
            }
            return nonstd::nullopt;
        }

        void ToJsonString(std::string& json_string)
        {
            try
            {
                nlohmann::json json = *static_cast<Derived*>(this);
                for (const auto& it : headers_map_)
                {
                    json[it.first] = VariantToJson(it.second);
                }
                json_string = json.dump();
            }
            catch (const std::exception&)
            {
                throw;
            }
        }

        void FromJsonString(const std::string& json_string)
        {
            try
            {
                const nlohmann::json json = nlohmann::json::parse(json_string);
                *static_cast<Derived*>(this) = json;
                for (const auto& item : json.items())
                {
                    if (Derived::JsonKeySet().count(item.key()) == 0)
                    {
                        headers_map_[item.key()] = JsonToVariant(item.value());
                    }
                }
            }
            catch (const std::exception&)
            {
                throw;
            }
        }

    protected:
        std::string module;
        std::string async_uuid;
        std::string msg_uuid;
        std::string route;
        std::string content_type;
        std::string timestamp;
        std::unordered_map<std::string, JsonValue> headers_map_;
    };
}

#endif
