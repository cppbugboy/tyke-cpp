/**
 * @file request_stub.h
 * @brief 请求存根声明。管理异步请求的回调函数、Future对象及超时清理。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <chrono>
#include <future>
#include <string>
#include <unordered_map>

#include "common/tyke_def.h"
#include "tyke_response.h"

namespace tyke
{
    
    class RequestStub
    {
    public:
        RequestStub() = delete;
        ~RequestStub() = delete;

        
        static void AddFuture(const std::string& uuid, std::promise<TykeResponse>& promise,
                              uint32_t timeout_ms = kDefaultStubTimeoutMs);

        
        static void SetFuture(const TykeResponse& response);

        
        static void AddFunc(const std::string& msg_uuid, const std::function<void(const TykeResponse &)>& func,
                            uint32_t timeout_ms = kDefaultStubTimeoutMs);

        
        static void ExecFunc(const TykeResponse& response);

        
        static void CleanupExpiredFuture(const std::string& uuid);

        
        static void CleanupExpiredFunc(const std::string& uuid);

    private:
        
        struct FutureEntry
        {
            std::promise<TykeResponse> promise;
            std::chrono::steady_clock::time_point created_at;
            uint32_t timeout_ms;
        };

        struct FuncEntry
        {
            std::function<void(const TykeResponse&)> func;
            std::chrono::steady_clock::time_point created_at;
            uint32_t timeout_ms;
        };

        inline static std::unordered_map<std::string, FutureEntry> uuid_future_map_;
        inline static std::mutex uuid_future_map_mutex_;

        inline static std::unordered_map<std::string, FuncEntry> uuid_func_map_;
        inline static std::mutex uuid_func_map_mutex_;
    };
}
