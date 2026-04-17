/**
 * @file tyke_utils.h
 * @brief Tyke框架通用工具函数声明
 * @author Nick
 * @date 2026/04/16
 *
 * 提供UUID生成、时间戳生成、UUID格式校验、临时目录获取等基础工具函数。
 */

#ifndef TYKE_UTILS_H
#define TYKE_UTILS_H

#include <string>

namespace tyke
{
    namespace utils
    {
        /**
         * @brief 生成UUID v4格式字符串
         * @return 符合UUID v4格式的字符串，如"550e8400-e29b-41d4-a716-446655440000"
         */
        std::string GenerateUUID();

        /**
         * @brief 生成毫秒精度的时间戳字符串
         * @return 格式为"YYYY-MM-DD HH:MM:SS.mmm"的时间戳字符串
         */
        std::string GenerateTimestamp();

        /**
         * @brief 校验字符串是否符合UUID格式
         * @param uuid 待校验的字符串
         * @return 符合UUID格式返回true，否则返回false
         */
        bool IsValidUUID(const std::string& uuid);

        /**
         * @brief 获取系统临时目录路径
         * @return 临时目录的绝对路径字符串，获取失败返回空字符串
         */
        std::string GetTempDir();
    }
}

#endif //TYKE_UTILS_H
