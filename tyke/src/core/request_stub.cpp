/**
 * @file request_stub.cpp
 * @brief 请求存根实现。管理异步请求的回调函数、Future对象及超时清理逻辑。
 * @author Nick
 * @date 2026/04/19
 */

#include "core/request_stub.h"
#include "core/tyke_response.h"
#include "component/timing_wheel.h"
#include "common/log_def.h"

namespace tyke
{
    void RequestStub::AddFuture(const std::string& uuid, std::promise<TykeResponse>& promise,
                                uint32_t timeout_ms)
    {
        std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
        uuid_future_map_.emplace(uuid, FutureEntry{std::move(promise), std::chrono::steady_clock::now(), timeout_ms});
        TimingWheel::GetInstance()->AddTask(uuid, timeout_ms, TaskEntry::kFuture);
        LOG_DEBUG("Future entry added, uuid={}, timeout={}ms", uuid, timeout_ms);
    }
    void RequestStub::SetFuture(const TykeResponse& response)
    {
        std::promise<TykeResponse> extracted_promise;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
            if (const auto it = uuid_future_map_.find(response.GetMsgUuid()); it != uuid_future_map_.end())
            {
                extracted_promise = std::move(it->second.promise);
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
            TimingWheel::GetInstance()->RemoveTask(response.GetMsgUuid());
            extracted_promise.set_value(response);
        }
    }

    void RequestStub::AddFunc(const std::string& msg_uuid, const std::function<void(const TykeResponse &)>& func,
                              uint32_t timeout_ms)
    {
        std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
        uuid_func_map_.emplace(msg_uuid, FuncEntry{func, std::chrono::steady_clock::now(), timeout_ms});
        TimingWheel::GetInstance()->AddTask(msg_uuid, timeout_ms, TaskEntry::kFunc);
        LOG_DEBUG("Callback entry added, uuid={}, timeout={}ms", msg_uuid, timeout_ms);
    }
    void RequestStub::ExecFunc(const TykeResponse& response)
    {
        std::function<void(const TykeResponse&)> extracted_func;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
            if (const auto it = uuid_func_map_.find(response.GetMsgUuid()); it != uuid_func_map_.end())
            {
                extracted_func = std::move(it->second.func);
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
            TimingWheel::GetInstance()->RemoveTask(response.GetMsgUuid());
            LOG_DEBUG("Executing callback for response, uuid={}", response.GetMsgUuid());
            extracted_func(response);
        }
    }

    void RequestStub::CleanupExpiredFuture(const std::string& uuid)
    {
        std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
        if (const auto it = uuid_future_map_.find(uuid); it != uuid_future_map_.end())
        {
            TykeResponse timeout_response;
            timeout_response.SetMsgUuid(uuid);
            timeout_response.SetResult(-1, "timeout");
            it->second.promise.set_value(timeout_response);
            uuid_future_map_.erase(it);
            LOG_WARN("Expired future cleaned up, uuid={}", uuid);
        }
    }

    void RequestStub::CleanupExpiredFunc(const std::string& uuid)
    {
        std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
        if (const auto it = uuid_func_map_.find(uuid); it != uuid_func_map_.end())
        {
            uuid_func_map_.erase(it);
            LOG_WARN("Expired func cleaned up, uuid={}", uuid);
        }
    }

    bool RequestStub::ExecFuncOrSetFuture(const TykeResponse& response)
    {
        const auto& uuid = response.GetMsgUuid();

        {
            std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
            if (const auto it = uuid_func_map_.find(uuid); it != uuid_func_map_.end())
            {
                auto extracted_func = std::move(it->second.func);
                uuid_func_map_.erase(it);
                TimingWheel::GetInstance()->RemoveTask(uuid);
                LOG_DEBUG("Executing fallback func for response, uuid={}", uuid);
                extracted_func(response);
                return true;
            }
        }

        {
            std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
            if (const auto it = uuid_future_map_.find(uuid); it != uuid_future_map_.end())
            {
                auto extracted_promise = std::move(it->second.promise);
                uuid_future_map_.erase(it);
                TimingWheel::GetInstance()->RemoveTask(uuid);
                LOG_DEBUG("Setting fallback future for response, uuid={}", uuid);
                extracted_promise.set_value(response);
                return true;
            }
        }

        return false;
    }
}