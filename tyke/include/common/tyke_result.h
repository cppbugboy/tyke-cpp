/**
 * @file tyke_result.h
 * @brief 通用返回值类型定义。基于nonstd::expected定义BoolResult、ByteVecResult等结果类型。
 * @author Nick
 * @date 2026/04/19
 */



#pragma once

#include <nonstd/expected.hpp>
#include <nonstd/optional.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace tyke {

template<typename T>
using Result = nonstd::expected<T, std::string>;

using BoolResult = nonstd::expected<bool, std::string>;

using ByteVecResult = nonstd::expected<std::vector<uint8_t>, std::string>;

}
