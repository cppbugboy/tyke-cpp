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
    /// 注册 Future 通道，等待指定 UUID 的响应。
    void AddFuture(const std::string& uuid, std::promise<Response>& promise);


    /// 将响应数据发送到匹配的 Future 通道。
    void SetFuture(const Response& response);

    void DeleteFuture(const std::string& uuid);


    /// 注册回调函数，等待指定 UUID 的响应。
    void AddFunc(const std::string& msg_uuid, const std::function<void(const Response &)>& func);

    /// 执行匹配的回调函数处理响应。
    void ExecFunc(const Response& response);

    void DeleteFunc(const std::string& msg_uuid);

    inline std::unordered_map<std::string, std::promise<Response>> uuid_future_map_;
    inline std::mutex uuid_future_map_mutex_;

    inline std::unordered_map<std::string, const std::function<void(const Response &)>&> uuid_func_map_;
    inline std::mutex uuid_func_map_mutex_;
}