/**
 * @file response_future.h
 * @brief 响应Future声明。封装异步响应的等待机制。
 * @author Nick
 * @date 2026/04/19
 */



#ifndef TYKE_RESPONSE_FUTURE_H
#define TYKE_RESPONSE_FUTURE_H
#include <future>
#include <string>

#include "tyke_response.h"

namespace tyke
{
    
    class ResponseFuture
    {
    public:
        
        ResponseFuture(const std::string& msg_uuid, std::future<TykeResponse> future);

        
        TykeResponse GetResponse();

    private:
        std::string msg_uuid_;
        std::future<TykeResponse> future_;
    };
}

#endif