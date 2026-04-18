/**
 * @file request_metadata.cpp
 * @brief 请求元数据实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现RequestMetadata类的具体逻辑，包括JSON序列化/反序列化等功能。
 */

#include "core/request_metadata.h"

namespace tyke
{
    /**
     * @brief 序列化为JSON字符串
     * @param json_string 输出的JSON字符串
     * @return 成功返回true，失败返回错误信息
     */
    void RequestMetadata::ToJsonString(std::string& json_string)
    {
        try
        {
            nlohmann::json json = *this;
            // 添加自定义元数据
            for (const auto& it : headers_map_)
            {
                json[it.first] = VariantToJson(it.second);
            }
            json_string = json.dump();
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(e.what());
        }
    }

    /**
     * @brief 从JSON字符串反序列化
     * @param json_string 输入的JSON字符串
     * @return 成功返回true，失败返回错误信息
     */
    void RequestMetadata::FromJsonString(const std::string& json_string)
    {
        try
        {
            const nlohmann::json json = nlohmann::json::parse(json_string);
            *this = json;
            for (const auto& item : json.items())
            {
                if (RequestMetadataJsonKeySet().count(item.key()) == 0)
                {
                    headers_map_[item.key()] = JsonToVariant(item.value());
                }
            }
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(e.what());
        }
    }

    std::string RequestMetadata::GetModule() const
    {
        return module;
    }

    RequestMetadata& RequestMetadata::SetModule(const std::string& in_module)
    {
        this->module = in_module;
        return *this;
    }


    std::string RequestMetadata::GetAsyncUuid() const
    {
        return async_uuid;
    }

    RequestMetadata& RequestMetadata::SetAsyncUuid(const std::string& in_async_uuid)
    {
        this->async_uuid = in_async_uuid;
        return *this;
    }

    std::string RequestMetadata::GetMsgUuid() const
    {
        return msg_uuid;
    }

    RequestMetadata& RequestMetadata::SetMsgUuid(const std::string& in_msg_uuid)
    {
        this->msg_uuid = in_msg_uuid;
        return *this;
    }

    std::string RequestMetadata::GetRoute() const
    {
        return route;
    }

    RequestMetadata& RequestMetadata::SetRoute(const std::string& in_route)
    {
        this->route = in_route;
        return *this;
    }


    std::string RequestMetadata::GetContentType() const
    {
        return content_type;
    }

    RequestMetadata& RequestMetadata::SetContentType(const std::string& in_content_type)
    {
        this->content_type = in_content_type;
        return *this;
    }

    std::string RequestMetadata::GetTimestamp() const
    {
        return timestamp;
    }

    RequestMetadata& RequestMetadata::SetTimestamp(const std::string& in_timestamp)
    {
        this->timestamp = in_timestamp;
        return *this;
    }

    /**
     * @brief 添加自定义元数据
     * @param key 元数据键名
     * @param value 元数据值
     * @return 成功返回true，失败返回错误信息
     */
    BoolResult RequestMetadata::AddMetadata(const std::string& key, const JsonValue& value)
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

    /**
     * @brief 获取自定义元数据
     * @param key 元数据键名
     * @return 存在返回值，不存在返回nullopt
     */
    nonstd::optional<JsonValue> RequestMetadata::GetMetadata(const std::string& key)
    {
        if (headers_map_.find(key) != headers_map_.end())
        {
            return headers_map_[key];
        }
        return nonstd::nullopt;
    }
} // tyke
