/**
 * @file response_metadata.h
 * @brief 响应元数据声明。继承MetadataBase，额外包含状态码和原因描述。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <nlohmann/json.hpp>

#include "core/metadata_base.h"

namespace tyke
{

    class ResponseMetadata : public MetadataBase<ResponseMetadata>
    {
    public:
        int GetStatus() const;
        ResponseMetadata& SetStatus(int status);

        const std::string& GetReason() const;
        ResponseMetadata& SetReason(std::string_view reason);

        friend void to_json(nlohmann::json& j, const ResponseMetadata& t);
        friend void from_json(const nlohmann::json& j, ResponseMetadata& t);

        static const std::unordered_set<std::string>& JsonKeySet();

    private:
        int status_ = 0;
        std::string reason_;
    };
}
