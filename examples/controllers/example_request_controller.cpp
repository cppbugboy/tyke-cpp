/**
 * @file example_request_controller.cpp
 * @brief 示例请求控制器实现。演示请求路由注册、请求处理和响应构建的完整流程。
 * @author Nick
 * @date 2026/04/19
 */

#include "example_request_controller.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>

void ExampleRequestController::RegisterMethod()
{
    fmt::print("注册请求路由处理器...\n");

    auto router = tyke::TykeFramework::GetRequestRouter();
    auto root = router->GetRoot();

    auto user_group = root->Group("/api/user");
    user_group->Route("/login", [this](const tyke::TykeRequest& req, tyke::TykeResponse& resp)
    {
        HandleUserLogin(req, resp);
    });
    user_group->Route("/logout", [this](const tyke::TykeRequest& req, tyke::TykeResponse& resp)
    {
        HandleUserLogout(req, resp);
    });

    auto data_group = root->Group("/api/data");
    data_group->Route("/query", [this](const tyke::TykeRequest& req, tyke::TykeResponse& resp)
    {
        HandleDataQuery(req, resp);
    });
    data_group->Route("/update", [this](const tyke::TykeRequest& req, tyke::TykeResponse& resp)
    {
        HandleDataUpdate(req, resp);
    });

    auto async_group = root->Group("/api/async");
    async_group->Route("/process", [this](const tyke::TykeRequest& req, tyke::TykeResponse& resp)
    {
        HandleAsyncProcess(req, resp);
    });

    fmt::print("✓ 请求路由处理器注册完成\n");
}

void ExampleRequestController::LogRequest(const tyke::TykeRequest& request, const std::string& handler_name)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    fmt::print("\n========================================\n");
    fmt::print("[{:%Y-%m-%d %H:%M:%S}] 请求处理器: {}\n", *std::localtime(&time_t), handler_name);
    fmt::print("========================================\n");
    fmt::print("消息UUID: {}\n", request.GetMsgUuid());
    fmt::print("模块: {}\n", request.GetModule());
    fmt::print("路由: {}\n", request.GetRoute());

    std::string content_type;
    std::vector<uint8_t> content;
    request.GetContent(content_type, content);

    if (content_type == "json" && !content.empty())
    {
        try
        {
            auto json_data = nlohmann::json::parse(content);
            fmt::print("请求内容: {}\n", json_data.dump(2));
        }
        catch (...)
        {
            fmt::print("请求内容: <解析失败>\n");
        }
    }
}

void ExampleRequestController::LogResponse(const tyke::TykeResponse& response, const std::string& handler_name)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    int status = 0;
    std::string reason;
    response.GetResult(status, reason);

    fmt::print("\n[{:%Y-%m-%d %H:%M:%S}] 响应已构建: {}\n", *std::localtime(&time_t), handler_name);
    fmt::print("状态码: {}\n", status);
    fmt::print("原因: {}\n", reason);
    fmt::print("========================================\n\n");
}

void ExampleRequestController::HandleUserLogin(const tyke::TykeRequest& request, tyke::TykeResponse& response)
{
    LogRequest(request, "HandleUserLogin");

    std::string content_type;
    std::vector<uint8_t> content;
    request.GetContent(content_type, content);

    if (content_type != "json")
    {
        response.SetResult(400, "Content type must be JSON");
        LogResponse(response, "HandleUserLogin");
        return;
    }

    try
    {
        auto json_data = nlohmann::json::parse(content);
        if (!json_data.contains("username") || !json_data.contains("password"))
        {
            response.SetResult(400, "Missing required fields: username, password");
            LogResponse(response, "HandleUserLogin");
            return;
        }

        std::string username = json_data["username"];
        std::string password = json_data["password"];

        if (username == "test_user" && password == "test_password")
        {
            nlohmann::json response_data = {
                {"success", true},
                {"user_id", "user_12345"},
                {"token", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"},
                {"expires_in", 3600}
            };
            std::vector<uint8_t> response_bytes = nlohmann::json::to_msgpack(response_data);
            std::string json_str = response_data.dump();
            response_bytes.assign(json_str.begin(), json_str.end());
            response.SetContent(tyke::ContentType::kJson, response_bytes);
            response.SetResult(200, "OK");
        }
        else
        {
            response.SetResult(401, "Invalid username or password");
        }
    }
    catch (const std::exception& e)
    {
        response.SetResult(400, "Invalid JSON format");
    }

    response.SetModule(request.GetModule());
    response.SetRoute(request.GetRoute());
    response.SetMsgUuid(request.GetMsgUuid());

    LogResponse(response, "HandleUserLogin");
}

void ExampleRequestController::HandleUserLogout(const tyke::TykeRequest& request, tyke::TykeResponse& response)
{
    LogRequest(request, "HandleUserLogout");

    nlohmann::json response_data = {
        {"success", true},
        {"message", "User logged out successfully"}
    };
    std::string json_str = response_data.dump();
    std::vector<uint8_t> response_bytes(json_str.begin(), json_str.end());
    response.SetContent(tyke::ContentType::kJson, response_bytes);
    response.SetResult(200, "OK");
    response.SetModule(request.GetModule());
    response.SetRoute(request.GetRoute());
    response.SetMsgUuid(request.GetMsgUuid());

    LogResponse(response, "HandleUserLogout");
}

void ExampleRequestController::HandleDataQuery(const tyke::TykeRequest& request, tyke::TykeResponse& response)
{
    LogRequest(request, "HandleDataQuery");

    nlohmann::json response_data = {
        {"success", true},
        {"total", 100},
        {
            "data", nlohmann::json::array({
                {{"id", 1}, {"name", "Item 1"}, {"status", "active"}},
                {{"id", 2}, {"name", "Item 2"}, {"status", "inactive"}},
                {{"id", 3}, {"name", "Item 3"}, {"status", "active"}}
            })
        }
    };
    std::string json_str = response_data.dump();
    std::vector<uint8_t> response_bytes(json_str.begin(), json_str.end());
    response.SetContent(tyke::ContentType::kJson, response_bytes);
    response.SetResult(200, "OK");
    response.SetModule(request.GetModule());
    response.SetRoute(request.GetRoute());
    response.SetMsgUuid(request.GetMsgUuid());

    LogResponse(response, "HandleDataQuery");
}

void ExampleRequestController::HandleDataUpdate(const tyke::TykeRequest& request, tyke::TykeResponse& response)
{
    LogRequest(request, "HandleDataUpdate");

    std::string content_type;
    std::vector<uint8_t> content;
    request.GetContent(content_type, content);

    if (content_type != "json")
    {
        response.SetResult(400, "Content type must be JSON");
        LogResponse(response, "HandleDataUpdate");
        return;
    }

    try
    {
        auto json_data = nlohmann::json::parse(content);
        if (!json_data.contains("id") || !json_data.contains("data"))
        {
            response.SetResult(400, "Missing required fields: id, data");
            LogResponse(response, "HandleDataUpdate");
            return;
        }
    }
    catch (const std::exception& e)
    {
        response.SetResult(400, "Invalid JSON format");
        LogResponse(response, "HandleDataUpdate");
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    nlohmann::json response_data = {
        {"success", true},
        {"message", "Data updated successfully"},
        {"updated_at", ms}
    };
    std::string json_str = response_data.dump();
    std::vector<uint8_t> response_bytes(json_str.begin(), json_str.end());
    response.SetContent(tyke::ContentType::kJson, response_bytes);
    response.SetResult(200, "OK");
    response.SetModule(request.GetModule());
    response.SetRoute(request.GetRoute());
    response.SetMsgUuid(request.GetMsgUuid());

    LogResponse(response, "HandleDataUpdate");
}

void ExampleRequestController::HandleAsyncProcess(const tyke::TykeRequest& request, tyke::TykeResponse& response)
{
    LogRequest(request, "HandleAsyncProcess");

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    nlohmann::json response_data = {
        {"success", true},
        {"task_id", fmt::format("task_{}", ms)},
        {"status", "processing"},
        {"async_uuid", request.GetAsyncUuid()}
    };
    std::string json_str = response_data.dump();
    std::vector<uint8_t> response_bytes(json_str.begin(), json_str.end());
    response.SetContent(tyke::ContentType::kJson, response_bytes);
    response.SetResult(202, "Accepted");
    response.SetModule(request.GetModule());
    response.SetRoute(request.GetRoute());
    response.SetMsgUuid(request.GetMsgUuid());
    response.SetAsyncUuid(request.GetAsyncUuid());

    LogResponse(response, "HandleAsyncProcess");
}
