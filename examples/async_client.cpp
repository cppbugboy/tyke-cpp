/**
 * @file async_client.cpp
 * @brief 异步请求客户端示例程序
 * @author Nick
 * @date 2026/04/19
 *
 * 演示如何使用Tyke客户端发送异步请求，包含三种异步请求方式：
 * 1. SendAsync - 通过ResponseRouter路由分发处理响应
 * 2. SendAsyncWithFunc - 通过回调函数处理响应
 * 3. SendAsyncWithFuture - 通过Future/Promise机制等待响应
 *
 * 异步请求的核心机制：
 * - 客户端启动独立的IPC Server监听异步响应
 * - 发送请求时通过SetAsyncUuid设置监听UUID
 * - 服务端根据async_uuid将响应发送到客户端监听服务器
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "fmt/base.h"
#include "tyke/tyke.h"
#include "core/data_handler.h"
#include "common/tyke_utils.h"

/**
 * @brief 服务端监听的UUID
 */
constexpr const char* SERVER_UUID = "39649d81-81c5-4f6e-b6a9-e768b55063be";

/**
 * @brief 用于同步等待响应的条件变量和计数器
 */
std::mutex g_mutex;
std::condition_variable g_cv;
std::atomic<int> g_response_count{0};
constexpr int TOTAL_ASYNC_REQUESTS = 3;

/**
 * @brief 响应处理函数（用于SendAsync方式）
 *
 * 此函数通过ResponseRouter注册，当收到kResponseAsync类型的响应时被调用。
 *
 * @param response 响应对象
 */
void OnAsyncResponse(const tyke::TykeResponse& response)
{
    std::string content_type;
    std::vector<unsigned char> content;
    response.GetContent(content_type, content);

    int status;
    std::string reason;
    response.GetResult(status, reason);

    fmt::println("[SendAsync响应] 收到异步响应:");
    fmt::println("  - 路由: {}", response.GetRoute());
    fmt::println("  - 状态码: {}", status);
    fmt::println("  - 内容: {}", std::string(content.begin(), content.end()));

    g_response_count++;
    g_cv.notify_one();
}

/**
 * @brief 注册响应路由处理器
 *
 * 对于SendAsync方式，需要在客户端注册响应路由处理器。
 * 当服务端返回响应时，ResponseRouter会根据路由路径找到对应的处理器。
 */
void RegisterResponseHandlers()
{
    // 获取响应路由器的根分组
    auto root_group = tyke::RESPONSE_ROUTER_INSTANCE->GetRoot();

    // 注册 /test/hello 路由的响应处理器
    root_group->AddRouteHandler("/test/hello", OnAsyncResponse);

    fmt::println("响应路由处理器已注册");
}

/**
 * @brief 演示SendAsync方式发送异步请求
 *
 * SendAsync方式的特点：
 * - 发送请求后立即返回，不阻塞当前线程
 * - 响应通过ResponseRouter路由分发到对应的处理器
 * - 适合需要统一响应处理逻辑的场景
 *
 * @param listen_uuid 客户端监听服务器的UUID
 */
void SendAsyncRequest(const std::string& listen_uuid)
{
    fmt::println("\n========== SendAsync 方式 ==========");

    auto request_ptr = tyke::MakeRequestPtr();

    std::string request_content = "hello from SendAsync";
    request_ptr->SetModule("test")
               .SetRoute("/test/hello")
               .SetContent(tyke::ContentType::kText,
                          std::vector<unsigned char>(request_content.begin(), request_content.end()));

    // 通过SetAsyncUuid设置async_uuid
    // 服务端会将响应发送到此UUID对应的IPC服务器
    request_ptr->SetAsyncUuid(listen_uuid);

    fmt::println("发送SendAsync请求: route={}, async_uuid={}",
                 request_ptr->GetRoute(), listen_uuid);

    // SendAsync第二个参数是recv_uuid，用于指定响应接收地址
    auto result = request_ptr->SendAsync(SERVER_UUID, listen_uuid);

    if (!result.has_value())
    {
        fmt::println("SendAsync请求失败: {}", result.error());
        g_response_count++;
        g_cv.notify_one();
    }
    else
    {
        fmt::println("SendAsync请求已发送，等待响应...");
    }
}

/**
 * @brief 演示SendAsyncWithFunc方式发送异步请求
 *
 * SendAsyncWithFunc方式的特点：
 * - 发送请求时注册回调函数
 * - 响应到达时自动调用回调函数
 * - 适合简单场景，直接处理响应
 *
 * @param listen_uuid 客户端监听服务器的UUID
 */
void SendAsyncWithFuncRequest(const std::string& listen_uuid)
{
    fmt::println("\n========== SendAsyncWithFunc 方式 ==========");

    auto request_ptr = tyke::MakeRequestPtr();

    std::string request_content = "hello from SendAsyncWithFunc";
    request_ptr->SetModule("test")
               .SetRoute("/test/hello")
               .SetContent(tyke::ContentType::kText,
                          std::vector<unsigned char>(request_content.begin(), request_content.end()));

    // 通过SetAsyncUuid设置async_uuid
    request_ptr->SetAsyncUuid(listen_uuid);

    fmt::println("发送SendAsyncWithFunc请求: route={}", request_ptr->GetRoute());

    // 发送请求并注册回调函数
    // 回调函数会在响应到达时被调用
    auto result = request_ptr->SendAsyncWithFunc(SERVER_UUID,
        [](const tyke::TykeResponse& response)
        {
            std::string content_type;
            std::vector<unsigned char> content;
            response.GetContent(content_type, content);

            int status;
            std::string reason;
            response.GetResult(status, reason);

            fmt::println("[SendAsyncWithFunc响应] 收到异步响应:");
            fmt::println("  - 路由: {}", response.GetRoute());
            fmt::println("  - 状态码: {}", status);
            fmt::println("  - 内容: {}", std::string(content.begin(), content.end()));

            g_response_count++;
            g_cv.notify_one();
        });

    if (!result.has_value())
    {
        fmt::println("SendAsyncWithFunc请求失败: {}", result.error());
        g_response_count++;
        g_cv.notify_one();
    }
    else
    {
        fmt::println("SendAsyncWithFunc请求已发送，等待回调...");
    }
}

/**
 * @brief 演示SendAsyncWithFuture方式发送异步请求
 *
 * SendAsyncWithFuture方式的特点：
 * - 返回ResponseFuture对象
 * - 可以在需要时调用GetResponse()阻塞等待响应
 * - 适合需要同步等待异步结果的场景
 *
 * @param listen_uuid 客户端监听服务器的UUID
 */
void SendAsyncWithFutureRequest(const std::string& listen_uuid)
{
    fmt::println("\n========== SendAsyncWithFuture 方式 ==========");

    auto request_ptr = tyke::MakeRequestPtr();

    std::string request_content = "hello from SendAsyncWithFuture";
    request_ptr->SetModule("test")
               .SetRoute("/test/hello")
               .SetContent(tyke::ContentType::kText,
                          std::vector<unsigned char>(request_content.begin(), request_content.end()));

    // 通过SetAsyncUuid设置async_uuid
    request_ptr->SetAsyncUuid(listen_uuid);

    fmt::println("发送SendAsyncWithFuture请求: route={}", request_ptr->GetRoute());

    // 发送请求并获取Future对象
    auto future_result = request_ptr->SendAsyncWithFuture(SERVER_UUID, listen_uuid);

    if (!future_result.has_value())
    {
        fmt::println("SendAsyncWithFuture请求失败: {}", future_result.error());
        g_response_count++;
        g_cv.notify_one();
        return;
    }

    fmt::println("SendAsyncWithFuture请求已发送，在另一个线程等待Future...");

    // 在单独的线程中等待Future结果
    std::thread future_thread([future = std::move(future_result.value())]() mutable
    {
        // GetResponse()会阻塞直到收到响应或超时
        tyke::TykeResponse response = future.GetResponse();

        std::string content_type;
        std::vector<unsigned char> content;
        response.GetContent(content_type, content);

        int status;
        std::string reason;
        response.GetResult(status, reason);

        fmt::println("[SendAsyncWithFuture响应] 收到异步响应:");
        fmt::println("  - 路由: {}", response.GetRoute());
        fmt::println("  - 状态码: {}", status);
        fmt::println("  - 内容: {}", std::string(content.begin(), content.end()));

        g_response_count++;
        g_cv.notify_one();
    });

    future_thread.detach();
}

/**
 * @brief 客户端数据回调函数
 *
 * 当监听IPC Server收到数据时，此回调函数被调用。
 * 这里使用框架提供的DataCallback处理数据，它会根据消息类型
 * 自动分发到相应的处理器（ResponseRouter、RequestStub等）。
 *
 * @param client_id 客户端ID
 * @param data_vec 接收到的数据
 * @param send_data_handler 发送数据的回调
 * @return 处理的字节数
 */
std::optional<uint32_t> ClientDataCallback(
    tyke::ClientId client_id,
    const std::vector<unsigned char>& data_vec,
    const tyke::SendDataHandler& send_data_handler)
{
    return tyke::data_handler::DataCallback(client_id, data_vec, send_data_handler);
}

int main()
{
    fmt::println("Tyke 异步请求客户端示例");
    fmt::println("====================================");

    try
    {
        // 生成客户端监听服务器的UUID
        // 服务端会将异步响应发送到此UUID
        std::string listen_uuid = tyke::utils::GenerateUUID();
        fmt::println("客户端监听UUID: {}", listen_uuid);

        // 注册响应路由处理器（用于SendAsync方式）
        RegisterResponseHandlers();

        // 创建并启动客户端监听IPC Server
        // 这是异步请求的关键：客户端需要有自己的IPC Server来接收异步响应
        tyke::IpcServer listen_server;
        auto start_result = listen_server.Start(listen_uuid, ClientDataCallback);

        if (!start_result)
        {
            fmt::println("启动监听服务器失败: {}", start_result.error());
            return 1;
        }

        fmt::println("客户端监听服务器已启动");

        // 发送三种异步请求
        SendAsyncRequest(listen_uuid);
        SendAsyncWithFuncRequest(listen_uuid);
        SendAsyncWithFutureRequest(listen_uuid);

        fmt::println("\n等待所有异步响应...");

        // 等待所有响应到达
        {
            std::unique_lock<std::mutex> lock(g_mutex);
            g_cv.wait(lock, []()
            {
                return g_response_count >= TOTAL_ASYNC_REQUESTS;
            });
        }

        fmt::println("\n所有异步响应已收到");

        // 停止监听服务器
        listen_server.Stop();
        fmt::println("客户端监听服务器已停止");
    }
    catch (const std::exception& e)
    {
        fmt::println("异常: {}", e.what());
        return 1;
    }

    fmt::println("====================================");
    fmt::println("异步请求示例完成");
    return 0;
}
