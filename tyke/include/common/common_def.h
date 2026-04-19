/**
 * @file common_def.h
 * @brief 通用类型定义与JSON转换工具
 * @author Nick
 * @date 2026/04/19
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
 * @brief JSON值变体类型
 *
 * 支持monostate(空)、bool、int、long long、double、string六种类型，
 * 用于元数据中动态类型值的存储。
 */
using JsonValue = nonstd::variant<nonstd::monostate, bool, int, long long, double, std::string>;

/**
 * @brief 将JsonValue转换为nlohmann::json对象
 * @param v 源变体值
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
 * @brief 将nlohmann::json对象转换为JsonValue
 * @param j 源JSON对象
 * @return 对应的变体值，无法识别的类型回退为字符串
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

#endif
