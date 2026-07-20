/**
 * @file request_stub.h
 * @brief 请求存根声明。管理异步请求的回调函数、Future对象及超时清理。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <string>

#include "response.h"

namespace tyke::stub
{
    /** @brief 注册异步等待的 future。当响应到达时通过 promise 传递结果。
     * @param uuid 用于匹配响应的消息UUID。
     * @param promise 用于设置异步结果的 promise。
     * @param timeout_ms 超时时间(毫秒)，超时后自动清理。
     */
    void AddFuture(const std::string& uuid, std::promise<Response>& promise,
                   uint32_t timeout_ms = kDefaultStubTimeoutMs);

    /** @brief 设置 future 的结果。响应到达时调用，匹配 uuid 并 set_value。 */
    void SetFuture(Response response);

    /** @brief 删除指定 uuid 的 future 条目。 */
    void DeleteFuture(const std::string& uuid);

    /** @brief 清理所有已超时的 future 条目。 */
    void CleanupExpiredFutures();

    /** @brief 注册异步回调函数。响应到达时执行回调。
     * @param msg_uuid 用于匹配响应的消息UUID。
     * @param func 响应处理回调函数。
     * @param timeout_ms 超时时间(毫秒)，超时后自动清理。
     */
    void AddFunc(const std::string& msg_uuid, const std::function<void(const Response &)>& func,
                 uint32_t timeout_ms = kDefaultStubTimeoutMs);

    /** @brief 执行匹配的回调函数。响应到达时调用，匹配 msg_uuid 并执行回调。 */
    void ExecFunc(const Response& response);

    /** @brief 删除指定 msg_uuid 的回调条目。 */
    void DeleteFunc(const std::string& msg_uuid);

    /** @brief 清理所有已超时的回调条目。 */
    void CleanupExpiredFuncs();
} // namespace tyke::stub