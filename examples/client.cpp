/**
 * @file client.cpp
 * @brief Tyke示例客户端程序。演示同步请求和三种异步请求方式（SendAsync、SendAsyncWithFunc、SendAsyncWithFuture）。
 * @author Nick
 * @date 2026/04/19
 */

#include <chrono>
#include <csignal>
#include <thread>

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>

#include "tyke/tyke.h"
#include "controllers/example_response_controller.h"

TYKE_CONTROLLER_REGISTER(ExampleResponseController)

static const std::string kServerUuid = "tyke_server_example";
static const std::string kClientListenerUuid = "tyke_client_listener";

static volatile bool g_running = true;

void SignalHandler(int signal)
{
    g_running = false;
}

void PrintRequestHeader(const std::string& title, const std::string& target_uuid, const tyke::TykeRequest& request)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    fmt::print("\n========================================\n");
    fmt::print("[{:%Y-%m-%d %H:%M:%S}] {}\n", *std::localtime(&time_t), title);
    fmt::print("========================================\n");
    fmt::print("目标UUID: {}\n", target_uuid);
    fmt::print("模块: {}\n", request.GetModule());
    fmt::print("路由: {}\n", request.GetRoute());
    if (!request.GetAsyncUuid().empty())
    {
        fmt::print("异步UUID: {}\n", request.GetAsyncUuid());
    }

    std::string content_type;
    std::vector<unsigned char> content;
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

void PrintSyncResponse(const tyke::TykeResponse& response)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    int status = 0;
    std::string reason;
    response.GetResult(status, reason);

    fmt::print("----------------------------------------\n");
    fmt::print("[{:%Y-%m-%d %H:%M:%S}] 收到同步响应\n", *std::localtime(&time_t));
    fmt::print("----------------------------------------\n");
    fmt::print("状态码: {}\n", status);
    fmt::print("原因: {}\n", reason);

    std::string content_type;
    std::vector<unsigned char> content;
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

void PrintAsyncResponse(const tyke::TykeResponse& response, const std::string& method_name)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    int status = 0;
    std::string reason;
    response.GetResult(status, reason);

    fmt::print("----------------------------------------\n");
    fmt::print("[{:%Y-%m-%d %H:%M:%S}] 收到异步响应 ({})\n", *std::localtime(&time_t), method_name);
    fmt::print("----------------------------------------\n");
    fmt::print("消息UUID: {}\n", response.GetMsgUuid());
    fmt::print("状态码: {}\n", status);
    fmt::print("原因: {}\n", reason);
    fmt::print("模块: {}\n", response.GetModule());
    fmt::print("路由: {}\n", response.GetRoute());

    std::string content_type;
    std::vector<unsigned char> content;
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

void DemoSyncRequest()
{
    fmt::print("\n>>> 1. 同步请求示例 (Send)\n");

    auto* request = tyke::TykeRequest::Acquire();
    request->SetModule("user_service");
    request->SetRoute("/api/user/login");

    nlohmann::json login_data = {
        {"username", "test_user"},
        {"password", "test_password"}
    };
    std::string json_str = login_data.dump();
    std::vector<unsigned char> content(json_str.begin(), json_str.end());
    request->SetContent(tyke::ContentType::kJson, content);

    request->AddMetadata("source", std::string("cpp_client"));
    request->AddMetadata("version", std::string("1.0"));

    PrintRequestHeader("发送同步请求", kServerUuid, *request);

    tyke::TykeResponse response;
    auto result = request->Send(kServerUuid, response);
    if (result.has_value())
    {
        PrintSyncResponse(response);
    }
    else
    {
        fmt::print("同步请求失败: {}\n", result.error());
    }

    tyke::TykeRequest::Release(request);
}

void DemoSendAsync()
{
    fmt::print("\n>>> 2. 异步请求示例 - SendAsync (即发即弃)\n");

    auto* request = tyke::TykeRequest::Acquire();
    request->SetModule("data_service");
    request->SetRoute("/api/async/process");
    request->SetAsyncUuid(kClientListenerUuid);

    nlohmann::json process_data = {
        {"task_type", "background_process"},
        {"priority", 1}
    };
    std::string json_str = process_data.dump();
    std::vector<unsigned char> content(json_str.begin(), json_str.end());
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

    tyke::TykeRequest::Release(request);
}

void DemoSendAsyncWithFunc()
{
    fmt::print("\n>>> 3. 异步请求示例 - SendAsyncWithFunc (回调函数)\n");

    auto* request = tyke::TykeRequest::Acquire();
    request->SetModule("data_service");
    request->SetRoute("/api/async/process");
    request->SetAsyncUuid(kClientListenerUuid);

    nlohmann::json process_data = {
        {"task_type", "callback_process"},
        {"priority", 2}
    };
    std::string json_str = process_data.dump();
    std::vector<unsigned char> content(json_str.begin(), json_str.end());
    request->SetContent(tyke::ContentType::kJson, content);

    PrintRequestHeader("发送异步请求 (SendAsyncWithFunc)", kServerUuid, *request);

    auto result = request->SendAsyncWithFunc(kServerUuid, [](const tyke::TykeResponse& response) {
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

    tyke::TykeRequest::Release(request);
}

void DemoSendAsyncWithFuture()
{
    fmt::print("\n>>> 4. 异步请求示例 - SendAsyncWithFuture (Future/Promise)\n");

    auto* request = tyke::TykeRequest::Acquire();
    request->SetModule("data_service");
    request->SetRoute("/api/async/process");
    request->SetAsyncUuid(kClientListenerUuid);

    nlohmann::json process_data = {
        {"task_type", "future_process"},
        {"priority", 3}
    };
    std::string json_str = process_data.dump();
    std::vector<unsigned char> content(json_str.begin(), json_str.end());
    request->SetContent(tyke::ContentType::kJson, content);

    PrintRequestHeader("发送异步请求 (SendAsyncWithFuture)", kServerUuid, *request);

    auto future_result = request->SendAsyncWithFuture(kServerUuid);
    if (future_result.has_value())
    {
        fmt::print("异步请求已发送，等待 Future 结果...\n");
        auto response = future_result.value().GetResponse();
        PrintAsyncResponse(response, "SendAsyncWithFuture");
    }
    else
    {
        fmt::print("异步请求失败: {}\n", future_result.error());
    }

    tyke::TykeRequest::Release(request);
}

int main()
{
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    fmt::print("========================================\n");
    fmt::print("  Tyke 示例客户端\n");
    fmt::print("========================================\n\n");

    auto* framework = tyke::App();
    framework->SetThreadPoolCount(4);
    framework->SetLogConfig("./tyke_client.log", "debug", 1024, 5);

    auto result = framework->Start(kClientListenerUuid);
    if (!result.has_value())
    {
        fmt::print("客户端启动失败: {}\n", result.error());
        return 1;
    }

    fmt::print("客户端监听已启动，UUID: {}\n\n", kClientListenerUuid);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // DemoSyncRequest();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // DemoSendAsync();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // DemoSendAsyncWithFunc();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    DemoSendAsyncWithFuture();

    fmt::print("\n等待异步响应处理完成...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    fmt::print("\n所有示例执行完毕\n");
    return 0;
}
