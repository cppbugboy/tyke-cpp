/**
 * @file response_future.cpp
 * @brief 响应Future实现。封装异步响应的等待和超时机制。
 * @author Nick
 * @date 2026/04/19
 */

#include "core/response_future.h"

namespace tyke
{
    ResponseFuture::ResponseFuture(const std::string& msg_uuid, std::future<TykeResponse> future)
        : msg_uuid_(msg_uuid), future_(std::move(future))
    {
    }
    TykeResponse ResponseFuture::GetResponse()
    {
        return future_.get();
    }
}