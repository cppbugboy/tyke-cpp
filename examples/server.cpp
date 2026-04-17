/**
 * @file server.cpp
 * @brief Tyke服务器示例程序
 * @author Nick
 * @date 2026/04/17
 *
 * 演示如何启动Tyke服务器并监听客户端连接。
 */

#include "tyke/tyke.h"
#include <fmt/format.h>
#include <thread>

int main()
{
    // 启动服务器，使用指定的UUID作为IPC端点标识
    auto start_result = tyke::App()->Start("39649d81-81c5-4f6e-b6a9-e768b55063be");
    if (!start_result)
    {
        fmt::print("start failed: {}\n", start_result.error());
        return 1;
    }

    // 服务器主循环
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
