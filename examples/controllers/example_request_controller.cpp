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

    /**
     * @brief 向框架的 RequestRouter 注册所有请求路由处理器。
     *
     * 路由注册在三个子组下：
     * - /api/user：登录和注销
     * - /api/data：查询和更新
     * - /api/async：后台过程模拟
     */
    void RegisterMethod()
    {
        fmt::print("注册请求路由处理器...\n");

        auto router = tyke::Framework::GetRequestRouter();
        const auto root = router.GetRoot();

        // 用户相关路由
        root->AddSubGroup("/api/user")->AddRouteHandler("/login", [](const tyke::Request& req, tyke::Response& resp)
        {
            HandleUserLogin(req, resp);
        });
        root->AddSubGroup("/api/user")->AddRouteHandler("/logout", [](const tyke::Request& req, tyke::Response& resp)
        {
            HandleUserLogout(req, resp);
        });

        // 数据相关路由
        root->AddSubGroup("/api/data")->AddRouteHandler("/query", [](const tyke::Request& req, tyke::Response& resp)
        {
            HandleDataQuery(req, resp);
        });
        root->AddSubGroup("/api/data")->AddRouteHandler("/update", [](const tyke::Request& req, tyke::Response& resp)
        {
            HandleDataUpdate(req, resp);
        });

        // 异步处理路由
        root->AddSubGroup("/api/async")->AddRouteHandler("/process", [](const tyke::Request& req, tyke::Response& resp)
        {
            HandleAsyncProcess(req, resp);
        });

        fmt::print("✓ 请求路由处理器注册完成\n");
    }

    /**
     * @brief 处理用户登录请求。
     *
     * 验证 JSON 内容中的用户名和密码字段，
     * 对照硬编码的测试值检查凭据，并返回
     * 带有模拟认证令牌的成功响应。
     *
     * @param request  包含 JSON 凭证的入站请求。
     * @param response 要填充登录结果的响应。
     *
     * @warning 此处理器仅用于演示，使用硬编码凭据。
     *          请勿在生产环境中使用。
     */
    void HandleUserLogin(const tyke::Request& request, tyke::Response& response)
    {
        std::string content_type;
        std::vector<uint8_t> content;
        request.GetContent(content_type, content);

        if (content_type != "json")
        {
            response.SetResult(tyke::StatusCode::kContentError, "Content type must be JSON");
            return;
        }

        try
        {
            auto json_data = nlohmann::json::parse(content);
            if (!json_data.contains("username") || !json_data.contains("password"))
            {
                response.SetResult(tyke::StatusCode::kContentError, "Missing required fields: username, password");
                return;
            }

            std::string username = json_data["username"];
            std::string password = json_data["password"];

            if (username == "test_user" && password == "test_password")
            {
                nlohmann::json response_data = {
                    {"success", true},
                    {"user_id", "user_12345"},
                    {"token", "<GENERATED_TOKEN_PLACEHOLDER>"},
                    {"expires_in", 3600}
                };
                std::string json_str = response_data.dump();
                std::vector<uint8_t> response_bytes(json_str.begin(), json_str.end());
                response.SetContent(tyke::ContentType::kJson, response_bytes);
                response.SetResult(tyke::StatusCode::kSuccess, "OK");
            }
            else
            {
                response.SetResult(tyke::StatusCode::kContentError, "Invalid username or password");
            }
        }
        catch (const std::exception& e)
        {
            response.SetResult(tyke::StatusCode::kContentError, "Invalid JSON format");
        }

        response.SetModule(request.GetModule());
        response.SetRoute(request.GetRoute());
        response.SetMsgUuid(request.GetMsgUuid());
    }

    /**
     * @brief 处理用户注销请求。
     * @param request  入站请求。
     * @param response 要填充注销确认的响应。
     */
    void HandleUserLogout(const tyke::Request& request, tyke::Response& response)
    {
        nlohmann::json response_data = {
            {"success", true},
            {"message", "User logged out successfully"}
        };
        std::string json_str = response_data.dump();
        std::vector<uint8_t> response_bytes(json_str.begin(), json_str.end());
        response.SetContent(tyke::ContentType::kJson, response_bytes);
        response.SetResult(tyke::StatusCode::kSuccess, "OK");
        response.SetModule(request.GetModule());
        response.SetRoute(request.GetRoute());
        response.SetMsgUuid(request.GetMsgUuid());
    }

    /**
     * @brief 处理数据查询请求，返回硬编码的示例数据集。
     * @param request  入站请求。
     * @param response 要填充示例数据数组的响应。
     */
    void HandleDataQuery(const tyke::Request& request, tyke::Response& response)
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
        response.SetResult(tyke::StatusCode::kSuccess, "OK");
        response.SetModule(request.GetModule());
        response.SetRoute(request.GetRoute());
        response.SetMsgUuid(request.GetMsgUuid());
    }

    /**
     * @brief 处理带字段验证的数据更新请求。
     *
     * 获取请求上下文用于潜在的异步操作，并验证
     * JSON 内容在确认更新之前包含必需的 "id" 和 "data" 字段。
     *
     * @param request  包含记录 ID 和更新负载的入站请求。
     * @param response 要填充更新确认和时间戳的响应。
     */
    void HandleDataUpdate(const tyke::Request& request, tyke::Response& response)
    {
        const auto context = request.GetContext();
        std::string content_type;
        std::vector<uint8_t> content;
        request.GetContent(content_type, content);

        if (content_type != "json")
        {
            response.SetResult(tyke::StatusCode::kContentError, "Content type must be JSON");
            return;
        }

        try
        {
            auto json_data = nlohmann::json::parse(content);
            if (!json_data.contains("id") || !json_data.contains("data"))
            {
                response.SetResult(tyke::StatusCode::kContentError, "Missing required fields: id, data");
                return;
            }
        }
        catch (const std::exception& e)
        {
            response.SetResult(tyke::StatusCode::kContentError, "Invalid JSON format");
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
        response.SetResult(tyke::StatusCode::kSuccess, "OK");
        response.SetModule(request.GetModule());
        response.SetRoute(request.GetRoute());
        response.SetMsgUuid(request.GetMsgUuid());
    }

    /**
     * @brief 处理异步后台任务请求。
     *
     * 从当前时间戳生成唯一任务 ID，并将异步 UUID 从请求复制到响应。
     * 此 UUID 由 IPC 层用于将异步响应路由回发起请求的客户端监听器。
     *
     * @param request  带有用于响应路由的异步 UUID 的入站请求。
     * @param response 要填充任务确认和异步 UUID 的响应。
     *
     * @note 响应的异步 UUID 必须与请求的异步 UUID 匹配，
     *       以便框架正确地将异步回复传递给客户端。
     */
    void HandleAsyncProcess(const tyke::Request& request, tyke::Response& response)
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
        response.SetResult(tyke::StatusCode::kSuccess, "Accepted");
        response.SetModule(request.GetModule());
        response.SetRoute(request.GetRoute());
        response.SetMsgUuid(request.GetMsgUuid());
        response.SetAsyncUuid(request.GetAsyncUuid());
    }
}