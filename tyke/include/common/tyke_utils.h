/**
 * @file tyke_utils.h
 * @brief 工具函数声明 (C++17)。提供UUID生成、时间戳生成、临时目录获取等通用工具函数。
 * @author Nick
 * @date 2026/04/19
 *
 * C++17特性:
 * - 使用std::string_view优化IsValidUUID参数
 * - 使用std::filesystem::temp_directory_path替代平台特定代码
 * - 使用嵌套命名空间tyke::utils
 */

#pragma once

#include <string>
#include <string_view>

namespace tyke::utils
{
    std::string GenerateUUID();


    std::string GenerateTimestamp();


    bool IsValidUUID(std::string_view uuid);


    std::string GetTempDir();

    bool IsValidServerName(std::string_view name);
} // namespace tyke::utils