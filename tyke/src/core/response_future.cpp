/**
 * @file response_future.cpp
 * @brief 响应Future对象实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现ResponseFuture类的具体逻辑，提供异步响应获取功能。
 */

#include "core/response_future.h"
#include "core/request_stub.h"

namespace tyke
{
    /**
     * @brief 构造函数
     * @param msg_uuid 消息UUID
     * @param future Future对象
     */
    ResponseFuture::ResponseFuture(const std::string& msg_uuid, std::future<TykeResponse> future)
        : msg_uuid_(msg_uuid), future_(std::move(future))
    {
    }

    /**
     * @brief 获取响应结果
     * @return 响应对象
     *
     * 阻塞等待响应到达，获取后自动清理RequestStub中的条目。
     */
    TykeResponse ResponseFuture::GetResponse()
    {
        auto response = future_.get();
        // 清理RequestStub中的条目
        RequestStub::DelFuture(msg_uuid_);
        return response;
    }
} // tyke