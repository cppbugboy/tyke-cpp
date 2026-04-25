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
void AddFuture(const std::string &uuid, std::promise<Response> &promise, uint32_t timeout_ms = kDefaultStubTimeoutMs);

void SetFuture(const Response &response);

void DeleteFuture(const std::string &uuid);

void CleanupExpiredFutures();

void AddFunc(const std::string &msg_uuid, const std::function<void(const Response &)> &func,
             uint32_t timeout_ms = kDefaultStubTimeoutMs);

void ExecFunc(const Response &response);

void DeleteFunc(const std::string &msg_uuid);

void CleanupExpiredFuncs();
}// namespace tyke::stub
