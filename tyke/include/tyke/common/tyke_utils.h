/**
 * @file tyke_utils.h
 * @brief 通用工具函数声明 (C++17)。提供UUID生成与校验、时间戳生成、临时目录获取等工具函数。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 工具函数集，位于tyke::utils命名空间中：
 * - GenerateUUID: 生成符合RFC 4122的随机UUID v4字符串
 * - GenerateTimestamp: 生成ISO 8601格式的时间戳字符串
 * - IsValidUUID: 校验UUID字符串格式是否合法
 * - GetTempDir: 获取系统临时目录路径
 * - IsValidServerName: 校验IPC服务名称是否合法
 *
 * C++17特性:
 * - 使用std::string_view优化IsValidUUID参数（零拷贝字符串检查）
 * - 使用std::filesystem::temp_directory_path替代平台特定代码
 * - 使用嵌套命名空间tyke::utils
 */

#pragma once

#include <string>
#include <string_view>

namespace tyke::utils
{
    /**
     * @brief 生成符合RFC 4122规范的随机UUID v4字符串
     * @return std::string 格式为 "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx" 的UUID字符串
     */
    std::string GenerateUUID();

    /**
     * @brief 生成当前UTC时间的ISO 8601格式时间戳
     * @return std::string 格式为 "YYYY-MM-DDTHH:MM:SS.mmmZ" 的时间戳字符串
     */
    std::string GenerateTimestamp();

    /**
     * @brief 校验字符串是否为合法的UUID v4格式
     * @param uuid 待校验的字符串
     * @return true 格式合法；false 格式不合法或为空
     * @note 仅校验格式（长度、连字符位置、十六进制字符），不校验版本位
     */
    bool IsValidUUID(std::string_view uuid);

    /**
     * @brief 获取系统临时目录路径
     * @return std::string 临时目录的绝对路径（末尾无路径分隔符）
     */
    std::string GetTempDir();

    /**
     * @brief 校验IPC服务名称是否合法
     * @param name 服务名称
     * @return true 名称合法（仅含字母、数字、下划线、连字符）；false 包含非法字符或为空
     * @note 服务名称长度受平台限制（Windows命名管道路径不超过256字符）
     */
    bool IsValidServerName(std::string_view name);
} // namespace tyke::utils
