/**
 * @file request_stub.cpp
 * @brief 请求存根实现。管理异步请求的回调函数、Future 对象及超时清理逻辑。
 *
 * 维护两组 UUID 映射：
 * - Future 映射：uuid -> promise<Response>，用于 SendAsyncWithFuture 模式
 * - Func 映射：uuid -> function<void(Response)>，用于 SendAsyncWithFunc 模式
 *
 * 每组映射配有独立的过期时间表，由框架的时间轮周期性触发 CleanupExpired 清理。
 *
 * @author Nick
 * @date 2026/04/19
 */

#include "tyke/core/request_stub.h"

#include <mutex>
#include <unordered_map>
#include <vector>

#include "tyke/common/log_def.h"
#include "tyke/component/timing_wheel.h"
#include "tyke/core/response.h"

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

    /**
     * @brief 注册 Future 条目。
     *
     * 将 promise 和过期时间分别存入映射表，由 CleanupExpiredFutures 在超时后清理。
     *
     * @param uuid 消息 UUID
     * @param promise 待设置的 promise（会被 move）
     * @param timeout_ms 超时时间（毫秒）
     */
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

    /**
     * @brief 设置 Future 结果并向等待者通知。
     *
     * 根据响应的 msg_uuid 查找对应的 promise 并调用 set_value。
     * 同时清除过期时间条目。
     *
     * @note promise.set_value 可能抛出 future_error（promise 已设置），捕获并记录警告。
     * @param response 响应对象（会被 move 到 promise 中）
     */
    void SetFuture(Response response)
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
            try
            {
                extracted_promise.set_value(std::move(response));
            }
            catch (const std::future_error& e)
            {
                LOG_WARN("SetFuture promise already satisfied, uuid={}, error={}", response.GetMsgUuid(), e.what());
            }
        }
    }

    /** @brief 强制删除 Future 条目（同时清除映射和过期时间）。 */
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

    /**
     * @brief 清理过期的 Future 条目。
     *
     * 遍历过期时间表，对每个过期条目：提取 promise 并设置为超时响应，
     * 然后从两个映射表中移除。
     *
     * @note 此函数由时间轮周期性调用，调用频率为 kDefaultStubTimeoutMs / 4。
     */
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

    /**
     * @brief 注册回调函数条目。
     *
     * @param msg_uuid 消息 UUID
     * @param func 响应回调函数
     * @param timeout_ms 超时时间（毫秒）
     */
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

    /**
     * @brief 执行已注册的回调函数。
     *
     * 根据响应的 msg_uuid 查找对应的回调并执行，然后清除条目。
     *
     * @param response 响应对象（const 引用，回调不应修改它）
     */
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

    /** @brief 强制删除回调函数条目。 */
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

    /** @brief 清理过期的回调函数条目（直接删除，不执行回调）。 */
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
