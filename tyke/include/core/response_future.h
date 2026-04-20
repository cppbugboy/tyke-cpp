/**
 * @file response_future.h
 * @brief 响应Future声明。封装异步响应的等待机制。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <future>
#include <string>

#include "tyke_response.h"

namespace tyke
{
    
    class ResponseFuture
    {
    public:
        
        /// 构造 ResponseFuture，绑定消息 UUID 和响应通道。
        ResponseFuture(const std::string& msg_uuid, std::future<TykeResponse> future);

        
        TykeResponse GetResponse();

        TykeResponse GetResponse(uint32_t timeout_ms);

    private:
        std::string msg_uuid_;
        std::future<TykeResponse> future_;
    };
}
