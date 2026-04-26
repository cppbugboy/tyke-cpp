/**
 * @file request_stub.cpp
 * @brief 请求存根实现。管理异步请求的回调函数、Future对象及超时清理逻辑。
 * @author Nick
 * @date 2026/04/19
 */

#include "core/request_stub.h"

#include <mutex>
#include <unordered_map>
#include <vector>

#include "common/log_def.h"
#include "component/timing_wheel.h"
#include "core/response.h"

namespace tyke::stub
{
    namespace
    {
        std::unordered_map<std::string, std::promise<Response>> uuid_future_map_;
        std::mutex uuid_future_map_mutex_;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> uuid_future_expire_map_;
        std::mutex uuid_future_expire_map_mutex_;

        std::unordered_map<std::string, std::function<void(const Response &)>> uuid_func_map_;
        std::mutex uuid_func_map_mutex_;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> uuid_func_expire_map_;
        std::mutex uuid_func_expire_map_mutex_;
    } // namespace

    void AddFuture(const std::string& uuid, std::promise<Response>& promise, const uint32_t timeout_ms)
    {
        {
            std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
            uuid_future_map_.emplace(uuid, std::move(promise));
        }
        {
            std::lock_guard<std::mutex> lock(uuid_future_expire_map_mutex_);
            uuid_future_expire_map_.emplace(
                uuid, std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms));
        }
        LOG_DEBUG("Future entry added, uuid={}, timeout={}ms", uuid, timeout_ms);
    }

    void SetFuture(const Response& response)
    {
        std::promise<Response> extracted_promise;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
            if (const auto it = uuid_future_map_.find(response.GetMsgUuid()); it != uuid_future_map_.end())
            {
                extracted_promise = std::move(it->second);
                uuid_future_map_.erase(it);
                found = true;
                LOG_DEBUG("Future result set, uuid={}", response.GetMsgUuid());
            }
            else
            {
                LOG_WARN("Future entry not found for response, uuid={}", response.GetMsgUuid());
            }
        }
        if (found)
        {
            {
                std::lock_guard<std::mutex> lock(uuid_future_expire_map_mutex_);
                uuid_future_expire_map_.erase(response.GetMsgUuid());
            }
            extracted_promise.set_value(std::move(response));
        }
    }

    void DeleteFuture(const std::string& uuid)
    {
        {
            std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
            uuid_future_map_.erase(uuid);
        }
        {
            std::lock_guard<std::mutex> lock(uuid_future_expire_map_mutex_);
            uuid_future_expire_map_.erase(uuid);
        }
        LOG_DEBUG("Future entry deleted, uuid={}", uuid);
    }

    void CleanupExpiredFutures()
    {
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::string> expired_uuids;
        {
            std::lock_guard<std::mutex> lock(uuid_future_expire_map_mutex_);
            for (const auto& [uuid, expire_time] : uuid_future_expire_map_)
            {
                if (now >= expire_time)
                {
                    expired_uuids.push_back(uuid);
                }
            }
        }
        for (const auto& uuid : expired_uuids)
        {
            std::promise<Response> extracted_promise;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
                if (const auto it = uuid_future_map_.find(uuid); it != uuid_future_map_.end())
                {
                    extracted_promise = std::move(it->second);
                    uuid_future_map_.erase(it);
                    found = true;
                }
            }
            {
                std::lock_guard<std::mutex> lock(uuid_future_expire_map_mutex_);
                uuid_future_expire_map_.erase(uuid);
            }
            if (found)
            {
                Response timeout_response;
                timeout_response.SetResult(StatusCode::kTimeout, "future timeout");
                timeout_response.SetMsgUuid(uuid);
                try
                {
                    extracted_promise.set_value(std::move(timeout_response));
                }
                catch (const std::future_error& e)
                {
                    LOG_WARN("Expired future promise set_value failed, uuid={}, error={}", uuid, e.what());
                }
                LOG_WARN("Expired future cleaned up, uuid={}", uuid);
            }
        }
    }

    void AddFunc(const std::string& msg_uuid, const std::function<void(const Response &)>& func, uint32_t timeout_ms)
    {
        {
            std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
            uuid_func_map_.emplace(msg_uuid, func);
        }
        {
            std::lock_guard<std::mutex> lock(uuid_func_expire_map_mutex_);
            uuid_func_expire_map_.emplace(msg_uuid,
                                          std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms));
        }
        LOG_DEBUG("Callback entry added, uuid={}, timeout={}ms", msg_uuid, timeout_ms);
    }

    void ExecFunc(const Response& response)
    {
        std::function < void(const Response &) > extracted_func;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
            if (const auto it = uuid_func_map_.find(response.GetMsgUuid()); it != uuid_func_map_.end())
            {
                extracted_func = it->second;
                uuid_func_map_.erase(it);
                found = true;
            }
            else
            {
                LOG_WARN("Callback entry not found for response, uuid={}", response.GetMsgUuid());
            }
        }
        if (found)
        {
            {
                std::lock_guard<std::mutex> lock(uuid_func_expire_map_mutex_);
                uuid_func_expire_map_.erase(response.GetMsgUuid());
            }
            LOG_DEBUG("Executing callback for response, uuid={}", response.GetMsgUuid());
            extracted_func(response);
        }
    }

    void DeleteFunc(const std::string& msg_uuid)
    {
        {
            std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
            uuid_func_map_.erase(msg_uuid);
        }
        {
            std::lock_guard<std::mutex> lock(uuid_func_expire_map_mutex_);
            uuid_func_expire_map_.erase(msg_uuid);
        }
        LOG_DEBUG("Callback entry deleted, uuid={}", msg_uuid);
    }

    void CleanupExpiredFuncs()
    {
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::string> expired_uuids;
        {
            std::lock_guard<std::mutex> lock(uuid_func_expire_map_mutex_);
            for (const auto& [uuid, expire_time] : uuid_func_expire_map_)
            {
                if (now >= expire_time)
                {
                    expired_uuids.push_back(uuid);
                }
            }
        }
        for (const auto& uuid : expired_uuids)
        {
            DeleteFunc(uuid);
            LOG_WARN("Expired callback cleaned up, uuid={}", uuid);
        }
    }
} // namespace tyke::stub