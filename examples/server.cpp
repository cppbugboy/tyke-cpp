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

/**
 * @brief 处理终止信号（SIGINT、SIGTERM），以优雅地关闭服务端。
 *
 * 将全局运行标志设为 false，使主循环退出并触发框架关闭序列。
 *
 * @param signal 接收到的信号编号。
 */
void SignalHandler(int signal)
{
    g_running = false;
    fmt::print("\n收到信号 {}，正在关闭服务端...\n", signal);
}

/**
 * @brief Tyke 示例服务端的入口点。
 *
 * 使用线程池和日志初始化 Tyke 框架，启动绑定到固定 UUID 的 IPC 服务端，
 * 并持续运行直到收到终止信号。
 *
 * @return 正常关闭返回 0，框架启动失败返回 1。
 */
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

    auto result = framework.Start("1879b1d8-8ab0-4542-8421-8d845eca6587");
    if (!result.has_value())
    {
        fmt::print("服务端启动失败: {}\n", result.error());
        return 1;
    }

    fmt::print("服务端已启动，监听UUID: 1879b1d8-8ab0-4542-8421-8d845eca6587\n");
    fmt::print("按 Ctrl+C 停止服务端...\n\n");

    while (g_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    framework.Shutdown();
    fmt::print("服务端已关闭\n");
    return 0;
}