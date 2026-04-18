/**
 * @file request_stub.h
 * @brief 请求存根
 * @author Nick
 * @date 2026/04/17
 *
 * RequestStub管理异步请求的回调函数和Future对象。
 * 用于处理异步请求的响应分发。
 */

#ifndef TYKE_REQUEST_STUB_H
#define TYKE_REQUEST_STUB_H
#include <chrono>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "tyke_response.h"

namespace tyke
{
    /**
     * @brief 请求存根类
     *
     * 管理异步请求的回调函数和Future对象，提供响应分发功能。
     * 支持回调函数方式和Future方式两种异步响应处理模式。
     */
    class RequestStub
    {
    public:
        RequestStub() = delete;
        ~RequestStub() = delete;

        /**
         * @brief 添加Future条目
         * @param uuid 消息UUID
         * @param promise Promise对象
         */
        static void AddFuture(const std::string& uuid, std::promise<TykeResponse>& promise);

        /**
         * @brief 设置Future结果
         * @param response 响应对象
         *
         * 根据响应的UUID找到对应的Future并设置结果。
         */
        static void SetFuture(const TykeResponse& response);

        /**
         * @brief 删除Future条目
         * @param msg_uuid 消息UUID
         */
        static void DelFuture(const std::string& msg_uuid);

        /**
         * @brief 添加回调函数条目
         * @param msg_uuid 消息UUID
         * @param func 回调函数
         */
        static void AddFunc(const std::string& msg_uuid, const std::function<void(const TykeResponse &)>& func);

        /**
         * @brief 执行回调函数
         * @param response 响应对象
         *
         * 根据响应的UUID找到对应的回调函数并执行。
         */
        static void ExecFunc(const TykeResponse& response);

        /**
         * @brief 删除Func条目
         * @param msg_uuid 消息UUID
         */
        static void DelFunc(const std::string& msg_uuid);
        /**
         * @brief 清理过期条目
         * @param timeout_ms 超时时间（毫秒），默认30秒
         *
         * 清理超过指定时间未收到响应的Future和回调函数条目。
         */
        static void CleanupExpired(uint32_t timeout_ms = 30000);

    private:
        /**
         * @brief Future条目结构
         */
        struct FutureEntry
        {
            std::promise<TykeResponse> promise;               ///< Promise对象
            std::chrono::steady_clock::time_point created_at; ///< 创建时间
        };

        /**
         * @brief 回调函数条目结构
         */
        struct FuncEntry
        {
            std::function<void(const TykeResponse&)> func;    ///< 回调函数
            std::chrono::steady_clock::time_point created_at; ///< 创建时间
        };

        static std::unordered_map<std::string, FutureEntry> uuid_future_map_;  ///< UUID到Future的映射
        static std::mutex uuid_future_map_mutex_;                              ///< Future映射互斥锁

        static std::unordered_map<std::string, FuncEntry> uuid_func_map_;      ///< UUID到回调函数的映射
        static std::mutex uuid_func_map_mutex_;                                ///< 回调函数映射互斥锁
    };
} // tyke

#endif //TYKE_REQUEST_STUB_H
