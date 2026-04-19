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

#include <chrono>
#include <cstring>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

namespace tyke
{
    namespace utils
    {
        std::string GenerateUUID()
        {

            std::random_device rd;
            std::mt19937 gen(rd());
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

            LOG_DEBUG("generated uuid: {}", ss.str());
            return ss.str();
        }

        std::string GenerateTimestamp()
        {
            const auto now = std::chrono::system_clock::now();

            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() %
                1000;
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

        bool IsValidUUID(const std::string& uuid)
        {

            static const std::regex uuid_regex("^\\{?[0-9a-fA-F]{8}-"
                "([0-9a-fA-F]{4}-){3}"
                "[0-9a-fA-F]{12}\\}?$");

            bool valid = std::regex_match(uuid, uuid_regex);
            LOG_DEBUG("uuid validation for '{}': {}", uuid, valid ? "valid" : "invalid");
            return valid;
        }

        std::string GetTempDir()
        {
#ifdef _WIN32
            char path[MAX_PATH] = {0};
            const DWORD length = GetTempPathA(MAX_PATH, path);
            if (length > 0)
            {
                LOG_DEBUG("temp dir: {}", path);
                return std::string(path);
            }
            LOG_WARN("failed to get temp dir on Windows");
#else

            const char* vars[] = {"TMPDIR", "TMP", "TEMP", "TEMPDIR"};
            for (const char* v : vars)
            {
                const char* path = std::getenv(v);
                if (path)
                {
                    LOG_DEBUG("temp dir from env {}: {}", v, path);
                    return std::string(path);
                }
            }
            LOG_DEBUG("temp dir fallback to /tmp");
            return "/tmp";
#endif
            return "";
        }
    }
}
