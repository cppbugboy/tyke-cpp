/**
 * @file request_stub.cpp
 * @brief 请求存根实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现RequestStub类的具体逻辑，管理异步请求的回调函数和Future对象。
 */

#include "core/request_stub.h"
#include "core/tyke_response.h"
#include "common/log_def.h"

namespace tyke
{
    // 静态成员初始化
    std::unordered_map<std::string, RequestStub::FutureEntry> RequestStub::uuid_future_map_;
    std::mutex RequestStub::uuid_future_map_mutex_;

    std::unordered_map<std::string, RequestStub::FuncEntry> RequestStub::uuid_func_map_;
    std::mutex RequestStub::uuid_func_map_mutex_;

    /**
     * @brief 添加Future条目
     * @param uuid 消息UUID
     * @param promise Promise对象
     */
    void RequestStub::AddFuture(const std::string& uuid, std::promise<TykeResponse>& promise)
    {
        std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
        uuid_future_map_.emplace(uuid, FutureEntry{std::move(promise), std::chrono::steady_clock::now()});
        LOG_DEBUG("Future entry added, uuid={}", uuid);
    }

    /**
     * @brief 设置Future结果
     * @param response 响应对象
     */
void RequestStub::SetFuture(const TykeResponse& response) {
        FutureEntry entry;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
            auto it = uuid_future_map_.find(response.metadata_.GetMsgUuid());
            if (it != uuid_future_map_.end()) {
                entry = std::move(it->second);
                uuid_future_map_.erase(it);
                found = true;
            }
        }
        if (found) {
            entry.promise.set_value(response);
        }
    }

    /**
     * @brief 删除Future条目
     * @param msg_uuid 消息UUID
     */
    void RequestStub::DelFuture(const std::string& msg_uuid)
    {
        std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
        uuid_future_map_.erase(msg_uuid);
        LOG_DEBUG("Future entry deleted, uuid={}", msg_uuid);
    }

    /**
     * @brief 添加回调函数条目
     * @param msg_uuid 消息UUID
     * @param func 回调函数
     */
    void RequestStub::AddFunc(const std::string& msg_uuid, const std::function<void(const TykeResponse &)>& func)
    {
        std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
        uuid_func_map_.emplace(msg_uuid, FuncEntry{func, std::chrono::steady_clock::now()});
        LOG_DEBUG("Callback entry added, uuid={}", msg_uuid);
    }

    /**
     * @brief 执行回调函数
     * @param response 响应对象
     */
    void RequestStub::ExecFunc(const TykeResponse& response)
    {
        std::unique_lock<std::mutex> lock(uuid_func_map_mutex_);
        auto it = uuid_func_map_.find(response.metadata_.GetMsgUuid());
        if (it != uuid_func_map_.end())
        {
            auto function = it->second.func;
            uuid_func_map_.erase(it);
            lock.unlock();
            LOG_DEBUG("Executing callback for response, uuid={}", response.metadata_.GetMsgUuid());
            function(response);
        }
        else
        {
            LOG_WARN("Callback entry not found for response, uuid={}", response.metadata_.GetMsgUuid());
        }
    }

    /**
     * @brief 删除Func条目
     * @param msg_uuid 消息UUID
     */
    void RequestStub::DelFunc(const std::string &msg_uuid)
    {
        std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
        uuid_func_map_.erase(msg_uuid);
        LOG_DEBUG("Func entry deleted, uuid={}", msg_uuid);
    }

    /**
     * @brief 清理过期条目
     * @param timeout_ms 超时时间（毫秒）
     */
    void RequestStub::CleanupExpired(uint32_t timeout_ms)
    {
        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(timeout_ms);

        // 清理过期的Future条目
        {
            std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
            for (auto it = uuid_future_map_.begin(); it != uuid_future_map_.end(); )
            {
                if (now - it->second.created_at > timeout)
                {
                    LOG_WARN("Future entry expired, uuid={}", it->first);
                    it = uuid_future_map_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // 清理过期的回调函数条目
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
} // tyke
