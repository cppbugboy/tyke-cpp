/**
 * @file tyke_utils.cpp
 * @brief 工具函数实现。提供 UUID 生成（v4 格式）、时间戳生成、临时目录获取、UUID/服务名校验。
 * @author Nick
 * @date 2026/04/19
 */

#include "tyke/common/tyke_utils.h"

#include "tyke/common/log_def.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>

namespace tyke::utils
{
    /**
     * @brief 生成符合 RFC 4122 v4 格式的 UUID 字符串。
     *
     * 格式：xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
     * 使用 thread_local mt19937 生成器避免每次调用创建 random_device 的开销。
     *
     * @return 36 字符 UUID 字符串（含连字符）
     */
    std::string GenerateUUID()
    {
        // thread_local static generator avoids the cost of creating random_device+mt19937
        // on every call. Seeded once per thread from std::random_device.
        thread_local std::mt19937 gen([]()
        {
            std::random_device rd;
            return std::mt19937(rd());
        }());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);

        std::stringstream ss;
        ss << std::hex;

        for (int i = 0; i < 8; i++)
            ss << dis(gen);
        ss << "-";

        for (int i = 0; i < 4; i++)
            ss << dis(gen);

        ss << "-4";

        for (int i = 0; i < 3; i++)
            ss << dis(gen);
        ss << "-";

        ss << dis2(gen);

        for (int i = 0; i < 3; i++)
            ss << dis(gen);
        ss << "-";

        for (int i = 0; i < 12; i++)
            ss << dis(gen);

        return ss.str();
    }

    /**
     * @brief 生成当前时间戳字符串。
     *
     * 格式：YYYY-MM-DD HH:MM:SS.mmm
     *
     * @return 时间戳字符串
     */
    std::string GenerateTimestamp()
    {
        const auto now = std::chrono::system_clock::now();

        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        auto timer = std::chrono::system_clock::to_time_t(now);

        std::tm bt{};
#ifdef _WIN32
        localtime_s(&bt, &timer);
#else
        localtime_r(&timer, &bt);
#endif

        std::ostringstream oss;
        oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms;

        return oss.str();
    }

    /**
     * @brief 校验 UUID 格式是否合法。
     *
     * 要求 36 字符长度，连字符在第 8/13/18/23 位，其余为十六进制字符。
     *
     * @param uuid 待校验的字符串
     * @return true 格式合法，false 格式非法
     */
    bool IsValidUUID(std::string_view uuid)
    {
        if (uuid.size() != 36)
            return false;
        for (size_t i = 0; i < uuid.size(); ++i)
        {
            const char c = uuid[i];
            if (i == 8 || i == 13 || i == 18 || i == 23)
            {
                if (c != '-')
                    return false;
            }
            else
            {
                if (!std::isxdigit(static_cast<unsigned char>(c)))
                    return false;
            }
        }
        return true;
    }

    /** @brief 获取系统临时目录路径。失败时返回空字符串并记录警告。 */
    std::string GetTempDir()
    {
        std::error_code ec;
        const auto temp = std::filesystem::temp_directory_path(ec);
        if (ec)
        {
            LOG_WARN("Failed to get temp dir: {}", ec.message());
            return "";
        }
        std::string temp_dir = temp.string();
        LOG_DEBUG("temp dir: {}", temp_dir);
        return temp_dir;
    }

    /**
     * @brief 校验 IPC 服务名称是否合法。
     *
     * 要求：非空、长度 <= 64、仅含字母数字下划线连字符。
     *
     * @param name 服务名称
     * @return true 合法，false 非法
     */
    bool IsValidServerName(std::string_view name)
    {
        if (name.empty() || name.size() > 64)
            return false;
        for (char c : name)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
                return false;
        }
        return true;
    }
} // namespace tyke::utils
