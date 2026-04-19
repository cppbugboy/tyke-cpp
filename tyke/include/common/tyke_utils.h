/**
 * @file tyke_utils.h
 * @brief 工具函数声明。提供UUID生成、时间戳生成、临时目录获取等通用工具函数。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef TYKE_UTILS_H
#define TYKE_UTILS_H

#include <string>

namespace tyke
{
    namespace utils
    {

        std::string GenerateUUID();


        std::string GenerateTimestamp();


        bool IsValidUUID(const std::string& uuid);


        std::string GetTempDir();
    }
}

#endif
