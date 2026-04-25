/**
 * @file response_future.cpp
 * @brief 响应Future实现。封装异步响应的等待和超时机制。
 * @author Nick
 * @date 2026/04/19
 */

#include <utility>

#include "core/response_future.h"

#include "common/log_def.h"
#include "common/tyke_def.h"

namespace tyke
{
    ResponseFuture::ResponseFuture(std::string msg_uuid, std::future<TykeResponse> future)
        : msg_uuid_(std::move(msg_uuid)), future_(std::move(future))
    {
    }

    TykeResponse ResponseFuture::GetResponse()
    {
        return GetResponse(kDefaultStubTimeoutMs);
    }

    TykeResponse ResponseFuture::GetResponse(uint32_t timeout_ms)
    {
        if (const auto status = future_.wait_for(std::chrono::milliseconds(timeout_ms)); status ==
            std::future_status::ready)
        {
            return future_.get();
        }
        LOG_WARN("GetResponse timeout, msg_uuid={}, timeout={}ms", msg_uuid_, timeout_ms);
        TykeResponse timeout_response;
        timeout_response.SetMsgUuid(msg_uuid_);
        timeout_response.SetResult(StatusCode::kNone, "timeout");
        return timeout_response;
    }
}