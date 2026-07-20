/**
 * @file client.cpp
 * @brief Tyke示例客户端程序。演示同步请求和三种异步请求方式（SendAsync、SendAsyncWithFunc、SendAsyncWithFuture）。
 * @author Nick
 * @date 2026/04/19
 */
#ifdef _WIN32
#include <mimalloc-new-delete.h>
#endif
#include "mimalloc.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>

#include "tyke/tyke.h"
#include "controllers/example_response_controller.h"

static const std::string kServerUuid = "1879b1d8-8ab0-4542-8421-8d845eca6587";
static const std::string kClientListenerUuid = "8b077afb-7287-4aeb-a39d-b7e0c327e30d";

static std::atomic<bool> g_running{true};

/**
 * @brief 处理终止信号（SIGINT、SIGTERM），以优雅地关闭客户端。
 * @param signal 接收到的信号编号。
 */
void SignalHandler(int signal)
{
    g_running = false;
}

/**
 * @brief 以线程安全的方式将 time_t 值转换为 tm 结构体。
 *
 * 在 Windows 上使用 localtime_s，在 POSIX 系统上使用 localtime_r。
 *
 * @param time 要转换的 time_t 值。
 * @return 表示本地时间的 tm 结构体。
 */
static tm SafeLocaltime(time_t time)
{
    tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    return tm_buf;
}

/**
 * @brief 在控制台上打印出站请求的格式化头部信息。
 *
 * 显示目标 UUID、模块、路由、可选的异步 UUID，
 * 并解析/美化打印请求中附带的任何 JSON 内容。
 *
 * @param title 描述请求类型的标签（例如 "同步请求"）。
 * @param target_uuid 目标端点的 UUID。
 * @param request 要打印详细信息的请求。
 */
void PrintRequestHeader(const std::string& title, const std::string& target_uuid, const tyke::Request& request)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    fmt::print("\n========================================\n");
    fmt::print("[{:%Y-%m-%d %H:%M:%S}] {}\n", SafeLocaltime(time_t), title);
    fmt::print("========================================\n");
    fmt::print("目标UUID: {}\n", target_uuid);
    fmt::print("模块: {}\n", request.GetModule());
    fmt::print("路由: {}\n", request.GetRoute());
    if (!request.GetAsyncUuid().empty())
    {
        fmt::print("异步UUID: {}\n", request.GetAsyncUuid());
    }

    std::string content_type;
    std::vector<uint8_t> content;
    request.GetContent(content_type, content);
    if (content_type == "json" && !content.empty())
    {
        try
        {
            auto json_data = nlohmann::json::parse(content);
            fmt::print("请求头: {{}}\n");
            fmt::print("请求体: {}\n", json_data.dump(2));
        }
        catch (...)
        {
            fmt::print("请求体: <解析失败>\n");
        }
    }
}

/**
 * @brief 在控制台上打印同步响应。
 *
 * 显示状态码、原因短语以及服务端返回的任何 JSON 内容。
 *
 * @param response 从服务端收到的同步响应。
 */
void PrintSyncResponse(const tyke::Response& response)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    auto status = tyke::StatusCode::kNone;
    std::string reason;
    response.GetResult(status, reason);

    fmt::print("----------------------------------------\n");
    fmt::print("[{:%Y-%m-%d %H:%M:%S}] 收到同步响应\n", SafeLocaltime(time_t));
    fmt::print("----------------------------------------\n");
    fmt::print("状态码: {}\n", (int)status);
    fmt::print("原因: {}\n", reason);

    std::string content_type;
    std::vector<uint8_t> content;
    response.GetContent(content_type, content);
    if (content_type == "json" && !content.empty())
    {
        try
        {
            auto json_data = nlohmann::json::parse(content);
            fmt::print("响应头: {{}}\n");
            fmt::print("响应体: {}\n", json_data.dump(2));
        }
        catch (...)
        {
            fmt::print("响应体: <解析失败>\n");
        }
    }
    fmt::print("========================================\n");
}

/**
 * @brief 在控制台上打印异步响应。
 *
 * 包含消息 UUID、状态码、原因、模块、路由和 JSON 内容。
 *
 * @param response 从服务端收到的异步响应。
 * @param method_name 标识哪个异步方法传递了此响应的标签。
 */
void PrintAsyncResponse(const tyke::Response& response, const std::string& method_name)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    auto status = tyke::StatusCode::kNone;
    std::string reason;
    response.GetResult(status, reason);

    fmt::print("----------------------------------------\n");
    fmt::print("[{:%Y-%m-%d %H:%M:%S}] 收到异步响应 ({})\n", SafeLocaltime(time_t), method_name);
    fmt::print("----------------------------------------\n");
    fmt::print("消息UUID: {}\n", response.GetMsgUuid());
    fmt::print("状态码: {}\n", (int)status);
    fmt::print("原因: {}\n", reason);
    fmt::print("模块: {}\n", response.GetModule());
    fmt::print("路由: {}\n", response.GetRoute());

    std::string content_type;
    std::vector<uint8_t> content;
    response.GetContent(content_type, content);
    if (content_type == "json" && !content.empty())
    {
        try
        {
            auto json_data = nlohmann::json::parse(content);
            fmt::print("响应头: {{}}\n");
            fmt::print("响应体: {}\n", json_data.dump(2));
        }
        catch (...)
        {
            fmt::print("响应体: <解析失败>\n");
        }
    }
    fmt::print("========================================\n");
}

/**
 * @brief 演示使用 Request::Send() 发送同步请求。
 *
 * 向服务端发送登录请求并等待直接响应。
 * 这是一个阻塞调用——调用线程将暂停，直到服务端回复。
 */
void DemoSyncRequest()
{
    fmt::print("\n>>> 1. 同步请求示例 (Send)\n");

    auto request = tyke::Request::Acquire();
    request->SetModule("user_service");
    request->SetRoute("/api/user/login");

    nlohmann::json login_data = {
        {"username", "test_user"},
        {"password", "test_password"}
    };
    std::string json_str = login_data.dump();
    std::vector<uint8_t> content(json_str.begin(), json_str.end());
    request->SetContent(tyke::ContentType::kJson, content);

    request->AddMetadata("source", std::string("cpp_client"));
    request->AddMetadata("version", std::string("1.0"));

    PrintRequestHeader("发送同步请求", kServerUuid, *request);

    tyke::Response response;
    auto result = request->Send(kServerUuid, response);
    if (result.has_value())
    {
        PrintSyncResponse(response);
    }
    else
    {
        fmt::print("同步请求失败: {}\n", result.error());
    }
}

/**
 * @brief 演示使用 Request::SendAsync() 发送即发即弃的异步请求。
 *
 * 发送请求后立即返回。响应到达时将由 ResponseRouter
 * 分发到匹配的响应控制器。
 *
 * @note 不附加回调或 future——响应完全通过响应控制器路由机制处理。
 */
void DemoSendAsync()
{
    fmt::print("\n>>> 2. 异步请求示例 - SendAsync (即发即弃)\n");

    auto request = tyke::Request::Acquire();
    request->SetModule("data_service");
    request->SetRoute("/api/async/process");
    request->SetAsyncUuid(kClientListenerUuid);

    nlohmann::json process_data = {
        {"task_type", "background_process"},
        {"priority", 1}
    };
    std::string json_str = process_data.dump();
    std::vector<uint8_t> content(json_str.begin(), json_str.end());
    request->SetContent(tyke::ContentType::kJson, content);

    PrintRequestHeader("发送异步请求 (SendAsync)", kServerUuid, *request);

    auto result = request->SendAsync(kServerUuid);
    if (result.has_value())
    {
        fmt::print("异步请求已发送，响应将由 ResponseRouter 分发到响应控制器\n");
    }
    else
    {
        fmt::print("异步请求失败: {}\n", result.error());
    }
}

/**
 * @brief 演示使用 SendAsyncWithFunc() 发送带 lambda 回调的异步请求。
 *
 * 当响应到达时，提供的回调将被调用，直接将响应对象传递给调用者的处理函数。
 */
void DemoSendAsyncWithFunc()
{
    fmt::print("\n>>> 3. 异步请求示例 - SendAsyncWithFunc (回调函数)\n");

    auto request = tyke::Request::Acquire();
    request->SetModule("data_service");
    request->SetRoute("/api/async/process");
    request->SetAsyncUuid(kClientListenerUuid);

    nlohmann::json process_data = {
        {"task_type", "callback_process"},
        {"priority", 2}
    };
    std::string json_str = process_data.dump();
    std::vector<uint8_t> content(json_str.begin(), json_str.end());
    request->SetContent(tyke::ContentType::kJson, content);

    PrintRequestHeader("发送异步请求 (SendAsyncWithFunc)", kServerUuid, *request);

    auto result = request->SendAsyncWithFunc(kServerUuid, [](const tyke::Response& response)
    {
        PrintAsyncResponse(response, "SendAsyncWithFunc 回调");
    });

    if (result.has_value())
    {
        fmt::print("异步请求已发送，等待回调执行...\n");
    }
    else
    {
        fmt::print("异步请求失败: {}\n", result.error());
    }
}

/**
 * @brief 演示使用 SendAsyncWithFuture() 通过 std::future 发送异步请求。
 *
 * 返回一个 future，调用者可以等待（阻塞或轮询）以在服务端处理完请求后获取响应。
 *
 * @note 这会通过 future::get() 阻塞调用线程，直到响应到达。
 */
void DemoSendAsyncWithFuture()
{
    fmt::print("\n>>> 4. 异步请求示例 - SendAsyncWithFuture (Future/Promise)\n");

    auto request = tyke::Request::Acquire();
    request->SetModule("data_service");
    request->SetRoute("/api/async/process");
    request->SetAsyncUuid(kClientListenerUuid);

    nlohmann::json process_data = {
        {"task_type", "future_process"},
        {"priority", 3}
    };
    std::string json_str = process_data.dump();
    std::vector<uint8_t> content(json_str.begin(), json_str.end());
    request->SetContent(tyke::ContentType::kJson, content);

    PrintRequestHeader("发送异步请求 (SendAsyncWithFuture)", kServerUuid, *request);

    auto future_result = request->SendAsyncWithFuture(kServerUuid);
    if (future_result.has_value())
    {
        fmt::print("异步请求已发送，等待 Future 结果...\n");
        auto response = future_result->get();
        PrintAsyncResponse(response, "SendAsyncWithFuture");
    }
    else
    {
        fmt::print("异步请求失败: {}\n", future_result.error());
    }
}

/**
 * @brief Tyke 示例客户端的入口点。
 *
 * 启动 Tyke 框架作为客户端监听器，然后按顺序运行四个演示场景：
 * 同步请求、即发即弃异步、基于回调的异步和基于 future 的异步。
 * 在关闭之前等待待处理的异步响应。
 *
 * @return 正常关闭返回 0，框架启动失败返回 1。
 */
int main()
{
    std::cout << "Using mimalloc version: " << mi_version() << std::endl;

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    fmt::print("========================================\n");
    fmt::print("  Tyke 示例客户端\n");
    fmt::print("========================================\n\n");

    auto& framework = tyke::App();
    framework.SetThreadPoolCount(4);
    framework.SetLogConfig("./tyke_client.log", "debug", 1024, 5);

    auto result = framework.Start(kClientListenerUuid);
    if (!result.has_value())
    {
        fmt::print("客户端启动失败: {}\n", result.error());
        return 1;
    }

    fmt::print("客户端监听已启动，UUID: {}\n\n", kClientListenerUuid);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    DemoSyncRequest();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    DemoSendAsync();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    DemoSendAsyncWithFunc();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    DemoSendAsyncWithFuture();

    fmt::print("\n等待异步响应处理完成...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    fmt::print("\n所有示例执行完毕\n");

    framework.Shutdown();
    return 0;
}