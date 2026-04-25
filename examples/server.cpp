/**
 * @file server.cpp
 * @brief Tyke示例服务端程序。启动IPC服务器，注册请求/响应控制器，处理客户端请求。
 * @author Nick
 * @date 2026/04/19
 */
#include "mimalloc.h"
#ifdef _WIN32
#include <mimalloc-new-delete.h>
#endif

#include <atomic>
#include <csignal>
#include <thread>

#include <fmt/format.h>

#include "tyke/tyke.h"
#include "controllers/example_request_controller.h"
#include "controllers/example_response_controller.h"

static std::atomic<bool> g_running{true};

void SignalHandler(int signal)
{
    g_running = false;
    fmt::print("\n收到信号 {}，正在关闭服务端...\n", signal);
}

int main()
{
    printf("Using mimalloc version: %d\n", mi_version());

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    fmt::print("========================================\n");
    fmt::print("  Tyke 示例服务端\n");
    fmt::print("========================================\n\n");

    auto& framework = tyke::App();
    framework.SetThreadPoolCount(4);
    framework.SetLogConfig("./tyke_server.log", "debug", 1024, 5);

    auto result = framework.Start("tyke_server_example");
    if (!result.has_value())
    {
        fmt::print("服务端启动失败: {}\n", result.error());
        return 1;
    }

    fmt::print("服务端已启动，监听UUID: tyke_server_example\n");
    fmt::print("按 Ctrl+C 停止服务端...\n\n");

    while (g_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    framework.Shutdown();
    fmt::print("服务端已关闭\n");
    return 0;
}