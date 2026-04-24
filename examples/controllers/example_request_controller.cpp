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

namespace controller::request::examples
{
    REQUEST_CONTROLLER_REGISTER(examples, RegisterMethod)
    void RegisterMethod()
    {
        fmt::print("注册请求路由处理器...\n");

        auto router = tyke::TykeFramework::GetRequestRouter();
        auto root = router->GetRoot();

        auto user_group = root->Group("/api/user");
        user_group->Route("/login", [](const tyke::TykeRequest& req, tyke::TykeResponse& resp, const std::shared_ptr<tyke::Context>& context)
        {
            HandleUserLogin(req, resp, context);
        });
        user_group->Route("/logout", [](const tyke::TykeRequest& req, tyke::TykeResponse& resp, const std::shared_ptr<tyke::Context>& context)
        {
            HandleUserLogout(req, resp, context);
        });

        auto data_group = root->Group("/api/data");
        data_group->Route("/query", [](const tyke::TykeRequest& req, tyke::TykeResponse& resp, const std::shared_ptr<tyke::Context>& context)
        {
            HandleDataQuery(req, resp, context);
        });
        data_group->Route("/update", [](const tyke::TykeRequest& req, tyke::TykeResponse& resp, const std::shared_ptr<tyke::Context>& context)
        {
            HandleDataUpdate(req, resp, context);
        });

        auto async_group = root->Group("/api/async");
        async_group->Route("/process", [](const tyke::TykeRequest& req, tyke::TykeResponse& resp, const std::shared_ptr<tyke::Context>& context)
        {
            HandleAsyncProcess(req, resp, context);
        });

        fmt::print("✓ 请求路由处理器注册完成\n");
    }

    void HandleUserLogin(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr)
    {
        std::string content_type;
        std::vector<uint8_t> content;
        request.GetContent(content_type, content);

        if (content_type != "json")
        {
            response.SetResult(400, "Content type must be JSON");
            return;
        }

        try
        {
            auto json_data = nlohmann::json::parse(content);
            if (!json_data.contains("username") || !json_data.contains("password"))
            {
                response.SetResult(400, "Missing required fields: username, password");
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
    }

    void HandleUserLogout(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr)
    {
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
    }

    void HandleDataQuery(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr)
    {
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
    }

    void HandleDataUpdate(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr)
    {
        std::string content_type;
        std::vector<uint8_t> content;
        request.GetContent(content_type, content);

        if (content_type != "json")
        {
            response.SetResult(400, "Content type must be JSON");
            return;
        }

        try
        {
            auto json_data = nlohmann::json::parse(content);
            if (!json_data.contains("id") || !json_data.contains("data"))
            {
                response.SetResult(400, "Missing required fields: id, data");
                return;
            }
        }
        catch (const std::exception& e)
        {
            response.SetResult(400, "Invalid JSON format");
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
    }

    void HandleAsyncProcess(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr)
    {
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
    }
}