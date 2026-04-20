/**
 * @file metadata_base.h
 * @brief 元数据模板基类 (C++17)。使用CRTP模式提取请求/响应元数据的公共字段和方法。
 * @author Nick
 * @date 2026/04/19
 *
 * C++17特性:
 * - 使用std::string_view优化setter和getter参数传递
 * - 使用std::optional替代nonstd::optional
 * - 使用结构化绑定遍历map
 * - 使用if初始化语句简化find操作
 */

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

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

        Derived& SetModule(std::string_view in_module)
        {
            module = in_module;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetAsyncUuid() const
        {
            return async_uuid;
        }

        Derived& SetAsyncUuid(std::string_view in_async_uuid)
        {
            async_uuid = in_async_uuid;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetMsgUuid() const
        {
            return msg_uuid;
        }

        Derived& SetMsgUuid(std::string_view in_msg_uuid)
        {
            msg_uuid = in_msg_uuid;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetRoute() const
        {
            return route;
        }

        Derived& SetRoute(std::string_view in_route)
        {
            route = in_route;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetContentType() const
        {
            return content_type;
        }

        Derived& SetContentType(std::string_view in_content_type)
        {
            content_type = in_content_type;
            return static_cast<Derived&>(*this);
        }

        const std::string& GetTimestamp() const
        {
            return timestamp;
        }

        Derived& SetTimestamp(std::string_view in_timestamp)
        {
            timestamp = in_timestamp;
            return static_cast<Derived&>(*this);
        }

        std::optional<bool> AddMetadata(std::string_view key, const JsonValue& value)
        {
            if (key.empty())
            {
                return std::nullopt;
            }
            try
            {
                headers_map_[std::string(key)] = value;
            }
            catch (...)
            {
                return std::nullopt;
            }
            return true;
        }

        std::optional<JsonValue> GetMetadata(std::string_view key) const
        {
            if (auto it = headers_map_.find(std::string(key)); it != headers_map_.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        nonstd::expected<bool, std::string> ToJsonString(std::string& json_string) const
        {
            try
            {
                nlohmann::json json = *static_cast<const Derived*>(this);
                for (const auto& [key, value] : headers_map_)
                {
                    json[key] = VariantToJson(value);
                }
                json_string = json.dump();
                return true;
            }
            catch (const nlohmann::json::exception& e)
            {
                return nonstd::make_unexpected(std::string("JSON serialization error: ") + e.what());
            }
            catch (const std::exception& e)
            {
                return nonstd::make_unexpected(std::string("Serialization failed: ") + e.what());
            }
        }

        nonstd::expected<bool, std::string> FromJsonString(const std::string& json_string)
        {
            try
            {
                const nlohmann::json json = nlohmann::json::parse(json_string);
                *static_cast<Derived*>(this) = json;
                for (const auto& [key, value] : json.items())
                {
                    if (Derived::JsonKeySet().count(key) == 0)
                    {
                        headers_map_[key] = JsonToVariant(value);
                    }
                }
                return true;
            }
            catch (const nlohmann::json::exception& e)
            {
                return nonstd::make_unexpected(std::string("JSON parse error: ") + e.what());
            }
            catch (const std::exception& e)
            {
                return nonstd::make_unexpected(std::string("Deserialization failed: ") + e.what());
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
