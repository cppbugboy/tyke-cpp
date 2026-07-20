/**
 * @file json_def.h
 * @brief JSON转换工具与动态类型定义 (C++17)。
 *        定义JsonValue变体类型及其与nlohmann::json之间的双向转换函数。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * JsonValue是一个类型安全的变体类型，支持以下JSON兼容类型：
 * - std::monostate (null)
 * - bool
 * - int
 * - long long
 * - double
 * - std::string
 *
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

/**
 * @brief JSON值变体类型，支持null、bool、int、long long、double、string六种类型
 */
using JsonValue = std::variant<std::monostate, bool, int, long long, double, std::string>;

/**
 * @brief 将JsonValue变体转换为nlohmann::json对象
 * @param v 源变体值
 * @return nlohmann::json 对应的JSON对象（null/boolean/integer/float/string）
 * @note 使用std::visit + constexpr if在编译期展开所有类型分支，无运行时开销
 */
inline nlohmann::json VariantToJson(const JsonValue& v)
{
    return std::visit(
        [](auto&& arg) -> nlohmann::json
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return nullptr;
            else if constexpr (std::is_same_v<T, bool>)
                return arg;
            else if constexpr (std::is_same_v<T, int>)
                return arg;
            else if constexpr (std::is_same_v<T, long long>)
                return arg;
            else if constexpr (std::is_same_v<T, double>)
                return arg;
            else if constexpr (std::is_same_v<T, std::string>)
                return arg;
        },
        v);
}

/**
 * @brief 将nlohmann::json对象转换为JsonValue变体
 * @param j 源JSON对象
 * @return JsonValue 对应的变体值
 * @note 按null -> bool -> integer -> float -> string顺序检查类型；
 *       无法匹配的类型会被dump为JSON字符串后存放
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