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

        
        /// 注册 Future 通道，等待指定 UUID 的响应。
        static void AddFuture(const std::string& uuid, std::promise<TykeResponse>& promise,
                              uint32_t timeout_ms = kDefaultStubTimeoutMs);

        
        /// 将响应数据发送到匹配的 Future 通道。
        static void SetFuture(const TykeResponse& response);

        
        /// 注册回调函数，等待指定 UUID 的响应。
        static void AddFunc(const std::string& msg_uuid, const std::function<void(const TykeResponse &)>& func,
                            uint32_t timeout_ms = kDefaultStubTimeoutMs);

        
        /// 执行匹配的回调函数处理响应。
        static void ExecFunc(const TykeResponse& response);

        
        /// 清理过期的 Future 通道条目。
        static void CleanupExpiredFuture(const std::string& uuid);

        
        /// 清理过期的回调函数条目。
        static void CleanupExpiredFunc(const std::string& uuid);

        /// 尝试将响应分发到已注册的Func回调或Future通道（兜底机制）。
        static bool ExecFuncOrSetFuture(const TykeResponse& response);

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
