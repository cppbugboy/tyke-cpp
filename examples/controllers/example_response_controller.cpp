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

    /**
     * @brief 向框架的 ResponseRouter 注册所有异步响应路由处理器。
     *
     * 在 /api/async 下设置三个路由：
     * - /process：通用异步响应处理
     * - /callback：回调特定的响应处理
     * - /notification：通知特定的响应处理
     *
     * /api/async 子组中的所有路由根据需要分发到 HandleAsyncCallback
     * 或 HandleAsyncNotification。
     */
    void RegisterMethod()
    {
        fmt::print("注册响应路由处理器...\n");

        auto router = tyke::Framework::GetResponseRouter();
        const auto root = router.GetRoot();

        const auto async_group = root->AddSubGroup("/api/async");
        async_group->AddRouteHandler("/process", [](const tyke::Response& resp)
        {
            HandleAsyncCallback(resp);
        });
        async_group->AddRouteHandler("/callback", [](const tyke::Response& resp)
        {
            HandleAsyncCallback(resp);
        });
        async_group->AddRouteHandler("/notification", [](const tyke::Response& resp)
        {
            HandleAsyncNotification(resp);
        });

        fmt::print("✓ 响应路由处理器注册完成\n");
    }

    /**
     * @brief 将异步响应的详细信息记录到控制台。
     *
     * 提取并格式化状态码、原因、消息 UUID、模块、路由
     * 以及任何 JSON 内容，以用于诊断显示。
     *
     * @param response     要记录的响应。
     * @param handler_name 标识哪个处理器调用了此日志记录器的标签。
     */
    void LogResponse(const tyke::Response& response, const std::string& handler_name)
    {
        const auto now = std::chrono::system_clock::now();
        const auto time_t = std::chrono::system_clock::to_time_t(now);

        auto status = tyke::StatusCode::kNone;
        std::string reason;
        response.GetResult(status, reason);

        fmt::print("\n========================================\n");
        fmt::print("========================================\n");
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
                fmt::print("响应内容: {}\n", json_data.dump(2));
            }
            catch (...)
            {
                fmt::print("响应内容: <解析失败>\n");
            }
        }
        fmt::print("========================================\n\n");
    }

    /**
     * @brief 处理异步回调响应。
     *
     * 记录响应详细信息，并模拟由服务端异步回复触发的业务逻辑执行。
     *
     * @param response 从服务端收到的异步响应。
     */
    void HandleAsyncCallback(const tyke::Response& response)
    {
        LogResponse(response, "HandleAsyncCallback");

        fmt::print("处理异步回调响应...\n");
        fmt::print("执行业务逻辑...\n");
        fmt::print("✓ 异步回调处理完成\n");
    }

    /**
     * @brief 处理异步通知响应。
     *
     * 记录响应详细信息，并模拟由服务端发送的通知触发的本地状态更新。
     *
     * @param response 从服务端收到的异步响应。
     */
    void HandleAsyncNotification(const tyke::Response& response)
    {
        LogResponse(response, "HandleAsyncNotification");

        fmt::print("处理异步通知响应...\n");
        fmt::print("更新本地状态...\n");
        fmt::print("✓ 异步通知处理完成\n");
    }
}