/**
 * @file large_transfer_test.cpp
 * @brief 大数据量传输测试。单连接写后读模式，支持大数据超时自适应。
 * @author Nick
 * @date 2026/07/21
 */

#ifdef _WIN32
#include <windows.h>
#else
#include <cstring>
#endif

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "tyke/ipc/ipc_client.h"
#include "tyke/ipc/ipc_server.h"
#include "tyke/common/log_def.h"
#include "tyke/component/thread_pool.h"

namespace {

uint32_t SimpleHash(const std::vector<uint8_t>& data)
{
    uint32_t h = 0x811C9DC5u;
    for (uint8_t b : data)
    {
        h ^= b;
        h *= 0x01000193u;
    }
    return h;
}

std::string FormatSize(size_t size)
{
    if (size < 1024)
        return std::to_string(size) + " B";
    if (size < 1024 * 1024)
        return std::to_string(size / 1024) + " KB";
    return std::to_string(size / (1024 * 1024)) + " MB";
}

void RunEchoServer(const std::string& name)
{
    std::cout << "[C++ 回显服务端] 正在启动，监听名称: '" << name << "'..." << std::endl;

    tyke::GetGlobalThreadPool().Init(8);

    tyke::IpcServer server;
    std::atomic<int64_t> msgCount{0};

    auto callback = [&](tyke::ClientId cid, const std::vector<uint8_t>& data,
                        auto sendCb) -> std::optional<uint32_t> {
        int64_t n = msgCount.fetch_add(1) + 1;
        std::cout << "[C++ 回显服务端] 收到第 " << n << " 条消息，大小: " << data.size() << " 字节，正在回显" << std::endl;
        sendCb(cid, data);
        return static_cast<uint32_t>(data.size());
    };

    auto result = server.Start(name, callback);
    if (!result.has_value())
    {
        std::cerr << "[C++ 回显服务端] 启动失败: " << result.error() << std::endl;
        exit(1);
    }
    std::cout << "[C++ 回显服务端] 启动成功，按 Ctrl+C 停止" << std::endl;
    std::cout << "[C++ 回显服务端] 已就绪" << std::endl;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void RunClient(const std::string& name)
{
    std::cout << "[C++ 客户端] 正在连接到 '" << name << "'..." << std::endl;

    tyke::GetGlobalThreadPool().Init(8);

    tyke::IpcConnection conn;
    auto cResult = conn.Connect(name, 10000);
    if (!cResult.has_value())
    {
        std::cerr << "[C++ 客户端] 连接失败: " << cResult.error() << std::endl;
        exit(1);
    }

    std::cout << "[C++ 客户端] 连接成功！" << std::endl;

    std::vector<size_t> sizes = {
        1, 64, 1024, 4096,
        64 * 1024 - 1,
        64 * 1024,
        64 * 1024 + 1,
        128 * 1024,
        256 * 1024,
        512 * 1024,
        768 * 1024,
        1024 * 1024,     // 1MB（最大限制）
    };

    int passed = 0;
    int failed = 0;

    for (size_t size : sizes)
    {
        std::string sizeName = FormatSize(size);

        std::vector<uint8_t> sendData(size);
        for (size_t i = 0; i < size; ++i)
        {
            sendData[i] = static_cast<uint8_t>((i % 251) + 1);
        }
        uint32_t expectedHash = SimpleHash(sendData);

        std::cout << "\n[C++ 客户端] 发送 " << size << " 字节 (" << sizeName << ")..." << std::endl;

        // 大消息使用更长超时
        uint32_t wrTimeout = 30000;
        uint32_t rdTimeout = 30000;
        if (size >= 1024 * 1024)
        {
            wrTimeout = 120000;
            rdTimeout = 120000;
        }

        auto wrResult = conn.Write(sendData.data(), sendData.size(), wrTimeout);
        if (!wrResult.has_value())
        {
            std::cout << "[C++ 客户端] ❌ 写入失败: " << wrResult.error() << std::endl;
            failed++;
            continue;
        }
        std::cout << "[C++ 客户端] 写入完成，等待回显..." << std::endl;

        std::vector<uint8_t> receivedData;
        auto rlResult = conn.ReadLoop(
            [&](const std::vector<uint8_t>& recv) -> bool {
                receivedData = recv;
                return true;
            },
            rdTimeout);

        if (!rlResult.has_value())
        {
            std::cout << "[C++ 客户端] ❌ 读取失败: " << rlResult.error() << std::endl;
            failed++;
            continue;
        }

        if (receivedData.size() != size)
        {
            std::cout << "[C++ 客户端] ❌ 字节数不匹配，期望 " << size << "，实际 " << receivedData.size() << std::endl;
            failed++;
            continue;
        }

        uint32_t receivedHash = SimpleHash(receivedData);
        if (receivedHash != expectedHash)
        {
            std::cout << "[C++ 客户端] ❌ 哈希校验失败" << std::endl;
            failed++;
            continue;
        }

        std::cout << "[C++ 客户端] ✅ 成功: " << sizeName << " (" << size << " 字节)，校验通过" << std::endl;
        passed++;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  结果: " << passed << "/" << (passed + failed) << " 通过" << std::endl;
    if (failed > 0)
    {
        std::cout << "  失败: " << failed << std::endl;
    }
    std::cout << "========================================" << std::endl;

    if (failed > 0)
    {
        exit(1);
    }
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "用法: large_transfer_test <模式> [服务端名称]" << std::endl;
        std::cout << "模式: cpp-echo-server, cpp-client" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string serverName = "large_transfer_test";
    if (argc > 2)
    {
        serverName = argv[2];
    }

    if (mode == "cpp-echo-server")
    {
        RunEchoServer(serverName);
    }
    else if (mode == "cpp-client")
    {
        RunClient(serverName);
    }
    else
    {
        std::cerr << "未知模式: " << mode << std::endl;
        return 1;
    }

    return 0;
}
