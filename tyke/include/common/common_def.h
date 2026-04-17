/**
 * @file common_def.h
 * @brief Tyke框架通用类型定义与JSON转换工具
 * @author Nick
 * @date 2026/04/16
 *
 * 定义JsonValue变体类型及其与nlohmann::json之间的转换函数，
 * 用于元数据(headers)的动态类型存储与JSON序列化/反序列化。
 */

#ifndef TYKE_COMMON_DEF_H
#define TYKE_COMMON_DEF_H
#include <unordered_set>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <nonstd/variant.hpp>

/**
 * @brief JSON值变体类型，支持多种基础数据类型的动态存储
 *
 * 可存储以下类型：空值(monostate)、布尔值、整数、长整数、浮点数、字符串。
 * 用于元数据(headers_map_)中值的统一表示。
 */
using JsonValue = nonstd::variant<nonstd::monostate, bool, int, long long, double, std::string>;

/**
 * @brief 请求元数据JSON序列化时需要排除的键集合
 *
 * 这些字段已通过NLOHMANN_DEFINE_TYPE_INTRUSIVE宏直接序列化，
 * 不需要额外写入headers_map_。
 */
inline const std::unordered_set<std::string>& RequestMetadataJsonKeySet()
{
    static const std::unordered_set<std::string> set = {
        "module", "msg_uuid", "route", "content_type", "timestamp"
    };
    return set;
}

/**
 * @brief 响应元数据JSON序列化时需要排除的键集合
 */
inline const std::unordered_set<std::string>& ResponseMetadataJsonKeySet()
{
    static const std::unordered_set<std::string> set = {
        "module", "msg_uuid", "route", "content_type", "timestamp", "status", "reason"
    };
    return set;
}

/**
 * @brief 将JsonValue变体转换为nlohmann::json对象
 * @param v 待转换的JsonValue值
 * @return 对应的JSON对象
 */
inline nlohmann::json VariantToJson(const JsonValue& v)
{
    switch (v.index())
    {
    case 0:
        return nullptr;
    case 1:
        return nonstd::get<1>(v);
    case 2:
        return nonstd::get<2>(v);
    case 3:
        return nonstd::get<3>(v);
    case 4:
        return nonstd::get<4>(v);
    case 5:
        return nonstd::get<5>(v);
    default:
        return nullptr;
    }
}

/**
 * @brief 将nlohmann::json对象转换为JsonValue变体
 * @param j 待转换的JSON对象
 * @return 对应的JsonValue值，无法识别的类型转为字符串兜底
 */
inline JsonValue JsonToVariant(const nlohmann::json& j)
{
    if (j.is_null())
        return nonstd::monostate{};
    if (j.is_boolean())
        return j.get<bool>();
    if (j.is_number_integer())
        return j.get<long long>();
    if (j.is_number_float())
        return j.get<double>();
    if (j.is_string())
        return j.get<std::string>();
    return j.dump();
}

#endif //TYKE_COMMON_DEF_H
