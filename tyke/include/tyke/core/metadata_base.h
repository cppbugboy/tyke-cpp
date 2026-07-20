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

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

#include "tyke/common/json_def.h"
#include "tyke/common/log_def.h"
#include "tyke/common/tyke_def.h"

namespace tyke
{
    /** @brief 元数据模板基类 (CRTP模式)。
     * @tparam Derived 派生类类型（RequestMetadata 或 ResponseMetadata）。
     *
     * 提供请求/响应共用的字段（模块、路由、UUID、时间戳、超时）及 JSON 序列化。
     * 使用 C++17 结构化绑定和 if-init-statement 优化序列化/查找操作。
     */
    template <typename Derived>
    class MetadataBase
    {
    public:
        /** @brief 获取目标模块名。 */
        [[nodiscard]] const std::string& GetModule() const
        {
            return module_;
        }

        /** @brief 设置目标模块名。返回派生类引用支持链式调用。 */
        Derived& SetModule(const std::string_view module)
        {
            module_ = module;
            return static_cast<Derived&>(*this);
        }

        /** @brief 获取异步回调UUID。 */
        [[nodiscard]] const std::string& GetAsyncUuid() const
        {
            return async_uuid_;
        }

        /** @brief 设置异步回调UUID。 */
        Derived& SetAsyncUuid(const std::string_view async_uuid)
        {
            async_uuid_ = async_uuid;
            return static_cast<Derived&>(*this);
        }

        /** @brief 获取消息UUID。 */
        [[nodiscard]] const std::string& GetMsgUuid() const
        {
            return msg_uuid_;
        }

        /** @brief 设置消息UUID。 */
        Derived& SetMsgUuid(const std::string_view msg_uuid)
        {
            msg_uuid_ = msg_uuid;
            return static_cast<Derived&>(*this);
        }

        /** @brief 获取路由路径。 */
        [[nodiscard]] const std::string& GetRoute() const
        {
            return route_;
        }

        /** @brief 设置路由路径。 */
        Derived& SetRoute(const std::string_view route)
        {
            route_ = route;
            return static_cast<Derived&>(*this);
        }

        /** @brief 获取内容类型。 */
        [[nodiscard]] const std::string& GetContentType() const
        {
            return content_type_;
        }

        /** @brief 设置内容类型。 */
        Derived& SetContentType(const std::string_view content_type)
        {
            content_type_ = content_type;
            return static_cast<Derived&>(*this);
        }

        /** @brief 获取时间戳。 */
        [[nodiscard]] const std::string& GetTimestamp() const
        {
            return timestamp_;
        }

        /** @brief 设置时间戳。 */
        Derived& SetTimestamp(const std::string_view timestamp)
        {
            timestamp_ = timestamp;
            return static_cast<Derived&>(*this);
        }

        /** @brief 获取超时时间(毫秒)。 */
        [[nodiscard]] uint64_t GetTimeout() const
        {
            return timeout_;
        }

        /** @brief 设置超时时间(毫秒)。 */
        Derived& SetTimeout(const uint64_t timeout)
        {
            timeout_ = timeout;
            return static_cast<Derived&>(*this);
        }

        /** @brief 添加自定义头部键值对。
         * @param key 键名（非空）。
         * @param value JSON 变体值。
         * @return 成功返回 true，键为空或异常时返回 nullopt。
         */
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
                LOG_ERROR("Unknown exception in AddMetadata for key={}", std::string(key));
                return std::nullopt;
            }
            return true;
        }

        /** @brief 获取自定义头部值。
         * @param key 键名。
         * @return 存在则返回对应 JSON 值，否则返回 nullopt。
         */
        [[nodiscard]] std::optional<JsonValue> GetMetadata(const std::string_view key) const
        {
            if (auto it = headers_map_.find(std::string(key)); it != headers_map_.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        /** @brief 将元数据序列化为 JSON 字符串。
         * @param json_string 输出 JSON 字符串。
         * @return 序列化成功返回 true，失败返回错误信息。
         * @note 已知字段通过 nlohmann::json 自动序列化，自定义头部按 Variant 类型转换。
         */
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

        /** @brief 从 JSON 字符串反序列化元数据。
         * @param json_string 输入的 JSON 字符串。
         * @return 反序列化成功返回 true，解析失败返回错误信息。
         * @note 已知字段通过 nlohmann::json 自动反序列化，未知键存入 headers_map_。
         */
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

        /** @brief 清除所有字段和自定义头部，恢复默认状态。 */
        void Clear()
        {
            module_.clear();
            async_uuid_.clear();
            msg_uuid_.clear();
            route_.clear();
            content_type_.clear();
            timestamp_.clear();
            timeout_ = 0;
            headers_map_.clear();
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
} // namespace tyke