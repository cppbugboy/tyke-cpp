/**
 * @file tyke_utils.cpp
 * @brief 工具函数实现。提供UUID生成、时间戳生成、临时目录获取等通用工具函数的实现。
 * @author Nick
 * @date 2026/04/19
 */

#include "common/tyke_utils.h"

#include "common/log_def.h"

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
std::string GenerateUUID()
{
    std::random_device              rd;
    std::mt19937                    gen(rd());
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

std::string GenerateTimestamp()
{
    const auto now = std::chrono::system_clock::now();

    const auto ms    = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    auto       timer = std::chrono::system_clock::to_time_t(now);

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

std::string GetTempDir()
{
    std::error_code ec;
    const auto      temp = std::filesystem::temp_directory_path(ec);
    if (ec)
    {
        LOG_WARN("Failed to get temp dir: {}", ec.message());
        return "";
    }
    std::string temp_dir = temp.string();
    LOG_DEBUG("temp dir: {}", temp_dir);
    return temp_dir;
}

bool IsValidServerName(std::string_view name)
{
    if (name.empty() || name.size() > 64)
        return false;
    for (char c: name)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
            return false;
    }
    return true;
}
}// namespace tyke::utils