/**
 * @file tyke_result.h
 * @brief Tyke框架结果类型别名定义
 * @author Nick
 * @date 2026/04/16
 *
 * 提供基于nonstd::expected和nonstd::optional的项目级类型别名，
 * 统一错误处理返回值类型，使调用方能显式处理成功与失败两种情况。
 * BoolResult: 用于返回bool结果或错误信息的函数
 * ByteVecResult: 用于返回字节数组或错误信息的函数
 * Result<T>: 通用的期望值类型别名
 */

#pragma once

#include <nonstd/expected.hpp>
#include <nonstd/optional.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace tyke {

/**
 * @brief 通用期望值类型，成功时包含T类型的值，失败时包含std::string错误信息
 */
template<typename T>
using Result = nonstd::expected<T, std::string>;

/**
 * @brief 布尔结果类型，成功时包含bool值，失败时包含std::string错误信息
 *
 * 适用于需要返回bool结果但同时需要传达失败原因的函数。
 * 成功路径返回BoolResult(true)或BoolResult(false)，
 * 失败路径返回nonstd::make_unexpected("错误描述")。
 */
using BoolResult = nonstd::expected<bool, std::string>;

/**
 * @brief 字节数组结果类型，成功时包含std::vector<uint8_t>，失败时包含std::string错误信息
 *
 * 适用于加解密等返回字节数组的操作，避免用空vector表示失败。
 */
using ByteVecResult = nonstd::expected<std::vector<uint8_t>, std::string>;

}
