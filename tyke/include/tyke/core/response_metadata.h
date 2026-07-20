/**
 * @file response_metadata.h
 * @brief 响应元数据声明。继承MetadataBase，额外包含状态码和原因描述。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_set>

#include "tyke/core/metadata_base.h"

namespace tyke
{
    /** @brief 响应元数据。继承 MetadataBase，额外包含状态码和原因描述。
     * @note 通过 CRTP 模式共享 MetadataBase 的通用字段和方法。
     */
    class ResponseMetadata : public MetadataBase<ResponseMetadata>
    {
    public:
        /** @brief 获取响应状态码。 */
        [[nodiscard]] StatusCode GetStatus() const;
        /** @brief 设置响应状态码。 */
        ResponseMetadata& SetStatus(StatusCode status);

        /** @brief 获取响应原因描述。 */
        [[nodiscard]] const std::string& GetReason() const;
        /** @brief 设置响应原因描述。 */
        ResponseMetadata& SetReason(std::string_view reason);

        friend void to_json(nlohmann::json& j, const ResponseMetadata& t);
        friend void from_json(const nlohmann::json& j, ResponseMetadata& t);

        /** @brief 返回已注册的 JSON 键名集合（含 status、reason）。 */
        static const std::unordered_set<std::string>& JsonKeySet();

    private:
        StatusCode status_ = StatusCode::kNone;
        std::string reason_;
    };
} // namespace tyke