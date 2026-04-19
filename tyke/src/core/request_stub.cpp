/**
 * @file request_stub.cpp
 * @brief 请求存根实现。管理异步请求的回调函数、Future对象及超时清理逻辑。
 * @author Nick
 * @date 2026/04/19
 */

#include "core/request_stub.h"
#include "core/tyke_response.h"
#include "common/log_def.h"

namespace tyke
{
    void RequestStub::AddFuture(const std::string& uuid, std::promise<TykeResponse>& promise)
    {
        std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
        uuid_future_map_.emplace(uuid, FutureEntry{std::move(promise), std::chrono::steady_clock::now()});
        LOG_DEBUG("Future entry added, uuid={}", uuid);
    }
    void RequestStub::SetFuture(const TykeResponse& response)
    {
        std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
        if (const auto it = uuid_future_map_.find(response.GetMsgUuid()); it != uuid_future_map_.end())
        {
            it->second.promise.set_value(response);
            uuid_future_map_.erase(it);
            LOG_DEBUG("Future result set, uuid={}", response.GetMsgUuid());
        }
        else
        {
            LOG_WARN("Future entry not found for response, uuid={}", response.GetMsgUuid());
        }
    }
    void RequestStub::DelFuture(const std::string& msg_uuid)
    {
        std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
        uuid_future_map_.erase(msg_uuid);
        LOG_DEBUG("Future entry deleted, uuid={}", msg_uuid);
    }
    void RequestStub::AddFunc(const std::string& msg_uuid, const std::function<void(const TykeResponse &)>& func)
    {
        std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
        uuid_func_map_.emplace(msg_uuid, FuncEntry{func, std::chrono::steady_clock::now()});
        LOG_DEBUG("Callback entry added, uuid={}", msg_uuid);
    }
    void RequestStub::ExecFunc(const TykeResponse& response)
    {
        std::unique_lock<std::mutex> lock(uuid_func_map_mutex_);
        if (const auto it = uuid_func_map_.find(response.GetMsgUuid()); it != uuid_func_map_.end())
        {
            const auto function = it->second.func;
            uuid_func_map_.erase(it);
            lock.unlock();
            LOG_DEBUG("Executing callback for response, uuid={}", response.GetMsgUuid());
            function(response);
        }
        else
        {
            LOG_WARN("Callback entry not found for response, uuid={}", response.GetMsgUuid());
        }
    }
    void RequestStub::CleanupExpired(const uint32_t timeout_ms)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(timeout_ms);

        {
            std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
            for (auto it = uuid_future_map_.begin(); it != uuid_future_map_.end(); )
            {
                if (now - it->second.created_at > timeout)
                {
                    LOG_WARN("Future entry expired, uuid={}", it->first);
                    try
                    {
                        TykeResponse timeout_resp;
                        timeout_resp.SetResult(kHttpStatusTimeout, "Request Timeout");
                        it->second.promise.set_value(timeout_resp);
                    }
                    catch (...)
                    {
                    }
                    it = uuid_future_map_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
            for (auto it = uuid_func_map_.begin(); it != uuid_func_map_.end(); )
            {
                if (now - it->second.created_at > timeout)
                {
                    LOG_WARN("Callback entry expired, uuid={}", it->first);
                    it = uuid_func_map_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
}
