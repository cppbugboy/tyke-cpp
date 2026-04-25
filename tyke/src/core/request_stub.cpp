/**
 * @file request_stub.cpp
 * @brief 请求存根实现。管理异步请求的回调函数、Future对象及超时清理逻辑。
 * @author Nick
 * @date 2026/04/19
 */

#include "core/request_stub.h"

#include "common/log_def.h"
#include "component/timing_wheel.h"
#include "core/response.h"

namespace tyke::stub
{
void AddFuture(const std::string &uuid, std::promise<Response> &promise)
{
    std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
    uuid_future_map_.emplace(uuid, std::move(promise));
    LOG_DEBUG("Future entry added, uuid={}", uuid);
}

void SetFuture(const Response &response)
{
    std::promise<Response> extracted_promise;
    bool                   found = false;
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
        extracted_promise.set_value(response);
    }
}

void DeleteFuture(const std::string &uuid)
{
    std::lock_guard<std::mutex> lock(uuid_future_map_mutex_);
    if (const auto it = uuid_future_map_.find(uuid); it != uuid_future_map_.end())
    {
        uuid_future_map_.erase(it);
    }
}

void AddFunc(const std::string &msg_uuid, const std::function<void(const Response &)> &func)
{
    std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
    uuid_func_map_.emplace(msg_uuid, func);
    LOG_DEBUG("Callback entry added, uuid={}", msg_uuid);
}

void ExecFunc(const Response &response)
{
    std::function<void(const Response &)> extracted_func;
    bool                                  found = false;
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
        LOG_DEBUG("Executing callback for response, uuid={}", response.GetMsgUuid());
        extracted_func(response);
    }
}

void DeleteFunc(const std::string &msg_uuid)
{
    std::lock_guard<std::mutex> lock(uuid_func_map_mutex_);
    uuid_func_map_.erase(msg_uuid);
    LOG_DEBUG("Callback entry added, uuid={}", msg_uuid);
}
}// namespace tyke::stub