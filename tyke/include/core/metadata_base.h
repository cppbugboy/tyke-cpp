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

#include "common/json_def.h"
#include "common/tyke_def.h"

namespace tyke
{
    template <typename Derived>
    class MetadataBase
    {
    public:
        [[nodiscard]] const std::string& GetModule() const
        {
            return module_;
        }

        Derived& SetModule(const std::string_view module)
        {
            module_ = module;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] const std::string& GetAsyncUuid() const
        {
            return async_uuid_;
        }

        Derived& SetAsyncUuid(const std::string_view async_uuid)
        {
            async_uuid_ = async_uuid;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] const std::string& GetMsgUuid() const
        {
            return msg_uuid_;
        }

        Derived& SetMsgUuid(const std::string_view msg_uuid)
        {
            msg_uuid_ = msg_uuid;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] const std::string& GetRoute() const
        {
            return route_;
        }

        Derived& SetRoute(const std::string_view route)
        {
            route_ = route;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] const std::string& GetContentType() const
        {
            return content_type_;
        }

        Derived& SetContentType(const std::string_view content_type)
        {
            content_type_ = content_type;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] const std::string& GetTimestamp() const
        {
            return timestamp_;
        }

        Derived& SetTimestamp(const std::string_view timestamp)
        {
            timestamp_ = timestamp;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] uint64_t GetTimeout() const
        {
            return timeout_;
        }

        Derived& SetTimeout(const uint64_t timeout)
        {
            timeout_ = timeout;
            return static_cast<Derived&>(*this);
        }

        std::optional<bool> AddMetadata(const std::string_view key, const JsonValue& value)
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

        [[nodiscard]] std::optional<JsonValue> GetMetadata(const std::string_view key) const
        {
            if (auto it = headers_map_.find(std::string(key)); it != headers_map_.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        BoolResult ToJsonString(std::string& json_string) const
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

        BoolResult FromJsonString(const std::string& json_string)
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
        std::string module_;
        std::string async_uuid_;
        std::string msg_uuid_;
        std::string route_;
        std::string content_type_;
        std::string timestamp_;
        uint64_t timeout_ = 0;
        std::unordered_map<std::string, JsonValue> headers_map_;
    };
}