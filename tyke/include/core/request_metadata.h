/**
 * @file request_metadata.h
 * @brief 请求元数据
 * @author Nick
 * @date 2026/04/17
 *
 * RequestMetadata封装了请求的元数据信息，包括模块名、路由、UUID等。
 * 支持JSON序列化/反序列化，用于协议编码。
 */

#ifndef TYKE_REQUEST_META_DATA_H
#define TYKE_REQUEST_META_DATA_H
#include <string>
#include <nlohmann/json.hpp>
#include "common/tyke_result.h"
#include "common/common_def.h"
#include "common/tyke_def.h"

namespace tyke
{
    /**
     * @brief 请求元数据类
     *
     * 存储请求的元数据信息，支持JSON序列化和反序列化。
     * 主要字段包括：模块名、异步UUID、消息UUID、路由、内容类型、时间戳。
     */
    class RequestMetadata
    {
    public:
        /**
         * @brief NLOHMANN JSON序列化宏
         *
         * 自动生成to_json和from_json函数，用于JSON序列化/反序列化。
         */
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(RequestMetadata, module, async_uuid, msg_uuid, route, content_type,
                                       timestamp)

        /**
         * @brief 序列化为JSON字符串
         * @param json_string 输出的JSON字符串
         * @return 成功返回true，失败返回错误信息
         */
        void ToJsonString(std::string& json_string);

        /**
         * @brief 从JSON字符串反序列化
         * @param json_string 输入的JSON字符串
         * @return 成功返回true，失败返回错误信息
         */
        void FromJsonString(const std::string& json_string);

        /**
         * @brief 获取模块名称
         * @return 模块名称字符串
         */
        std::string GetModule() const;

        /**
         * @brief 设置模块名称
         * @param in_module 模块名称
         * @return 当前元数据引用，支持链式调用
         */
        RequestMetadata& SetModule(const std::string& in_module);

        /**
         * @brief 获取异步UUID
         * @return 异步UUID字符串
         */
        std::string GetAsyncUuid() const;

        /**
         * @brief 设置异步UUID
         * @param in_async_uuid 异步UUID
         * @return 当前元数据引用，支持链式调用
         */
        RequestMetadata& SetAsyncUuid(const std::string& in_async_uuid);

        /**
         * @brief 获取消息UUID
         * @return 消息UUID字符串
         */
        std::string GetMsgUuid() const;

        /**
         * @brief 设置消息UUID
         * @param in_msg_uuid 消息UUID
         * @return 当前元数据引用，支持链式调用
         */
        RequestMetadata& SetMsgUuid(const std::string& in_msg_uuid);

        /**
         * @brief 获取路由路径
         * @return 路由路径字符串
         */
        std::string GetRoute() const;

        /**
         * @brief 设置路由路径
         * @param in_route 路由路径
         * @return 当前元数据引用，支持链式调用
         */
        RequestMetadata& SetRoute(const std::string& in_route);

        /**
         * @brief 获取内容类型
         * @return 内容类型字符串
         */
        std::string GetContentType() const;

        /**
         * @brief 设置内容类型
         * @param in_content_type 内容类型字符串
         * @return 当前元数据引用，支持链式调用
         */
        RequestMetadata& SetContentType(const std::string& in_content_type);

        /**
         * @brief 获取时间戳
         * @return 时间戳字符串
         */
        std::string GetTimestamp() const;

        /**
         * @brief 设置时间戳
         * @param in_timestamp 时间戳字符串
         * @return 当前元数据引用，支持链式调用
         */
        RequestMetadata& SetTimestamp(const std::string& in_timestamp);

        /**
         * @brief 添加自定义元数据
         * @param key 元数据键名
         * @param value 元数据值
         * @return 成功返回true，失败返回错误信息
         */
        nonstd::expected<bool, std::string> AddMetadata(const std::string& key, const JsonValue& value);

        /**
         * @brief 获取自定义元数据
         * @param key 元数据键名
         * @return 存在返回值，不存在返回nullopt
         */
        nonstd::optional<JsonValue> GetMetadata(const std::string& key);

    private:
        std::string module;                                 ///< 模块名称
        std::string async_uuid;                              ///< 异步UUID
        std::string msg_uuid;                                ///< 消息UUID
        std::string route;                                   ///< 路由路径
        std::string content_type;                            ///< 内容类型
        std::string timestamp;                               ///< 时间戳
        std::unordered_map<std::string, JsonValue> headers_map_;  ///< 自定义元数据映射
    };
} // tyke

#endif //TYKE_REQUEST_META_DATA_H