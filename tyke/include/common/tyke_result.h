/**
 * @file tyke_result.h
 * @brief 通用返回值类型定义 (C++17)
 * @author Nick
 * @date 2026/04/19
 *
 * 基于nonstd::expected定义BoolResult、ByteVecResult等结果类型。
 *
 * 注意: nonstd::expected保留使用，因为std::expected是C++23特性。
 * 使用std::optional替代了nonstd::optional。
 */


#pragma once

#include <nonstd/expected.hpp>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace tyke {

template<typename T>
using Result = nonstd::expected<T, std::string>;

using BoolResult = nonstd::expected<bool, std::string>;

using ByteVecResult = nonstd::expected<std::vector<uint8_t>, std::string>;

}
