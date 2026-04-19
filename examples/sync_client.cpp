/**
 * @file sync_client.cpp
 * @brief 同步请求客户端示例程序
 * @author Nick
 * @date 2026/04/19
 *
 * 演示如何使用Tyke客户端发送同步请求并等待响应。
 * 同步请求会阻塞当前线程，直到收到服务端的响应或超时。
 */

#include <iostream>
#include <string>
#include <vector>

#include "fmt/base.h"
#include "tyke/tyke.h"

/**
 * @brief 服务端监听的UUID
 * 客户端需要使用此UUID连接到服务端
 */
constexpr const char* SERVER_UUID = "39649d81-81c5-4f6e-b6a9-e768b55063be";

/**
 * @brief 发送同步请求的示例函数
 *
 * 同步请求的工作流程：
 * 1. 从对象池获取请求和响应对象
 * 2. 设置请求参数（模块、路由、内容）
 * 3. 调用Send方法发送请求并等待响应
 * 4. 处理响应结果
 */
void SendSyncRequest()
{
    fmt::println("========== 同步请求示例 ==========");

    // 从对象池获取请求和响应对象
    // 使用智能指针管理对象生命周期，退出作用域时自动归还对象池
    auto request_ptr = tyke::MakeRequestPtr();
    auto response_ptr = tyke::MakeResponsePtr();

    // 设置请求内容
    std::string request_content = "hello from sync client";
    request_ptr->SetModule("test")
               .SetRoute("/test/hello")
               .SetContent(tyke::ContentType::kText,
                          std::vector<unsigned char>(request_content.begin(), request_content.end()));

    fmt::println("发送同步请求: route={}, content={}",
                 request_ptr->GetRoute(), request_content);

    // 发送同步请求
    // Send方法会阻塞当前线程，直到收到响应或发生错误
    auto result = request_ptr->Send(SERVER_UUID, *response_ptr);

    if (!result.has_value())
    {
        fmt::println("同步请求失败: {}", result.error());
        return;
    }

    // 获取响应内容
    std::string content_type;
    std::vector<unsigned char> content;
    response_ptr->GetContent(content_type, content);

    // 获取响应状态
    int status;
    std::string reason;
    response_ptr->GetResult(status, reason);

    fmt::println("同步响应收到:");
    fmt::println("  - 状态码: {}", status);
    fmt::println("  - 原因: {}", reason);
    fmt::println("  - 内容类型: {}", content_type);
    fmt::println("  - 内容: {}", std::string(content.begin(), content.end()));
    fmt::println("  - 消息UUID: {}", response_ptr->GetMsgUuid());
}

int main()
{
    fmt::println("Tyke 同步请求客户端示例");
    fmt::println("====================================");

    try
    {
        SendSyncRequest();
    }
    catch (const std::exception& e)
    {
        fmt::println("异常: {}", e.what());
        return 1;
    }

    fmt::println("====================================");
    fmt::println("同步请求示例完成");
    return 0;
}
