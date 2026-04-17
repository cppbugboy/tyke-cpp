/**
 * @file client.cpp
 * @brief Tyke客户端示例程序
 * @author Nick
 * @date 2026/04/17
 *
 * 演示如何使用Tyke客户端发送请求并接收响应。
 */

#include <iostream>
#include <thread>

#include "fmt/base.h"

#include "tyke/tyke.h"

int main()
{
    const std::string str = "hello world ";

    // 从对象池获取请求和响应对象
    auto tyke_response_ptr = tyke::MakeResponsePtr();
    auto tyke_request_ptr  = tyke::MakeRequestPtr();

    // 设置请求参数
    tyke_request_ptr->SetModule("test").SetRoute("/test/hello").SetContent(
            tyke::ContentType::kText, std::vector<unsigned char>(str.begin(), str.end()));

    // 同步发送请求
    auto expected = tyke_request_ptr->Send("39649d81-81c5-4f6e-b6a9-e768b55063be", *tyke_response_ptr);
    if (!expected.has_value())
    {
        fmt::print("expected error: {}", expected.error());
    }

    // 获取响应内容
    std::string                content_type;
    std::vector<unsigned char> content;
    tyke_response_ptr->GetContent(content_type, content);
    fmt::println("resp_content_type: {}", content_type);
    fmt::println("resp_content: {}", std::string(content.begin(), content.end()));
}
