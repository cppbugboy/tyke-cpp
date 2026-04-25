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

#include "response.h"

namespace tyke::stub
{
void AddFuture(const std::string &uuid, std::promise<Response> &promise, uint32_t timeout_ms = kDefaultStubTimeoutMs);

void SetFuture(const Response &response);

void DeleteFuture(const std::string &uuid);

void CleanupExpiredFutures();

void AddFunc(const std::string &msg_uuid, const std::function<void(const Response &)> &func, uint32_t timeout_ms = kDefaultStubTimeoutMs);

void ExecFunc(const Response &response);

void DeleteFunc(const std::string &msg_uuid);

void CleanupExpiredFuncs();

inline std::unordered_map<std::string, std::promise<Response>>                uuid_future_map_;
inline std::mutex                                                              uuid_future_map_mutex_;
inline std::unordered_map<std::string, std::chrono::steady_clock::time_point> uuid_future_expire_map_;
inline std::mutex                                                              uuid_future_expire_map_mutex_;

inline std::unordered_map<std::string, std::function<void(const Response &)>> uuid_func_map_;
inline std::mutex                                                             uuid_func_map_mutex_;
inline std::unordered_map<std::string, std::chrono::steady_clock::time_point> uuid_func_expire_map_;
inline std::mutex                                                             uuid_func_expire_map_mutex_;
}// namespace tyke::stub
