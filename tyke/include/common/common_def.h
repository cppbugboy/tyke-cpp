/**
 * @file common_def.h
 * @brief 通用类型定义与JSON转换工具 (C++17)
 * @author Nick
 * @date 2026/04/19
 *
 * 定义JsonValue变体类型及其与nlohmann::json之间的转换函数，
 * 用于元数据(headers)的动态类型存储与JSON序列化/反序列化。
 *
 * C++17特性:
 * - 使用std::variant替代nonstd::variant
 * - 使用std::visit + constexpr if替代switch-case进行类型分发
 * - 使用std::monostate替代nonstd::monostate
 */

#pragma once

#include <nlohmann/json.hpp>
#include <variant>

using JsonValue = std::variant<std::monostate, bool, int, long long, double, std::string>;

/**
 * @brief 将JsonValue转换为nlohmann::json对象
 * @param v 源变体值
 * @return 对应的JSON对象
 */
inline nlohmann::json VariantToJson(const JsonValue& v)
{
    return std::visit([](auto&& arg) -> nlohmann::json {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) return nullptr;
        else if constexpr (std::is_same_v<T, bool>) return arg;
        else if constexpr (std::is_same_v<T, int>) return arg;
        else if constexpr (std::is_same_v<T, long long>) return arg;
        else if constexpr (std::is_same_v<T, double>) return arg;
        else if constexpr (std::is_same_v<T, std::string>) return arg;
    }, v);
}

/**
 * @brief 将nlohmann::json对象转换为JsonValue
 * @param j 源JSON对象
 * @return 对应的变体值，无法识别的类型回退为字符串
 */
inline JsonValue JsonToVariant(const nlohmann::json& j)
{
    if (j.is_null())
        return std::monostate{};
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

