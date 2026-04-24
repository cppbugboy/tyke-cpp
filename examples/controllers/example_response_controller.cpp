/**
 * @file example_response_controller.cpp
 * @brief 示例响应控制器实现。演示异步响应的路由注册和处理完整流程。
 * @author Nick
 * @date 2026/04/19
 */

#include "example_response_controller.h"

#include <chrono>
#include <ctime>

#include <nlohmann/json.hpp>
#include <fmt/format.h>

namespace controller::response::examples
{
    RESPONSE_CONTROLLER_REGISTER(examples, RegisterMethod)
    void RegisterMethod()
    {
        fmt::print("注册响应路由处理器...\n");

        const auto router = tyke::TykeFramework::GetResponseRouter();
        const auto root = router->GetRoot();

        const auto async_group = root->Group("/api/async");
        async_group->Route("/callback", [](const tyke::TykeResponse& resp)
        {
            HandleAsyncCallback(resp);
        });
        async_group->Route("/process", [](const tyke::TykeResponse& resp)
        {
            HandleAsyncCallback(resp);
        });
        async_group->Route("/notification", [](const tyke::TykeResponse& resp)
        {
            HandleAsyncNotification(resp);
        });

        fmt::print("✓ 响应路由处理器注册完成\n");
    }

    void LogResponse(const tyke::TykeResponse& response, const std::string& handler_name)
    {
        const auto now = std::chrono::system_clock::now();
        const auto time_t = std::chrono::system_clock::to_time_t(now);

        int status = 0;
        std::string reason;
        response.GetResult(status, reason);

        fmt::print("\n========================================\n");
        fmt::print("========================================\n");
        fmt::print("消息UUID: {}\n", response.GetMsgUuid());
        fmt::print("状态码: {}\n", status);
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
                fmt::print("响应内容: {}\n", json_data.dump(2));
            }
            catch (...)
            {
                fmt::print("响应内容: <解析失败>\n");
            }
        }
        fmt::print("========================================\n\n");
    }

    void HandleAsyncCallback(const tyke::TykeResponse& response)
    {
        LogResponse(response, "HandleAsyncCallback");

        fmt::print("处理异步回调响应...\n");
        fmt::print("执行业务逻辑...\n");
        fmt::print("✓ 异步回调处理完成\n");
    }

    void HandleAsyncNotification(const tyke::TykeResponse& response)
    {
        LogResponse(response, "HandleAsyncNotification");

        fmt::print("处理异步通知响应...\n");
        fmt::print("更新本地状态...\n");
        fmt::print("✓ 异步通知处理完成\n");
    }
}