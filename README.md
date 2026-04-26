# Tyke C++ - 高性能跨平台IPC框架

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-green.svg)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)]()

## 概述

Tyke 是一个高性能、跨平台的本地进程间通信（IPC）框架，提供安全加密的双向通信能力。支持 Windows 命名管道和 Linux Unix 域套接字，内置 ECDH 密钥交换和 AES-256-GCM 加密。

## 功能特性

- **跨平台支持**: Windows (Named Pipe + IOCP) / Linux (Unix Domain Socket + epoll)
- **安全通信**: ECDH P-256 密钥交换 + AES-256-GCM 认证加密
- **高性能**: IOCP/epoll 异步 I/O，连接池复用
- **大消息支持**: 应用层分片机制，支持最大 16MB 消息
- **易用 API**: 简洁的同步/异步接口

## 环境要求

### Windows

- Windows 10/11
- CMake 3.16+
- MinGW-w64 或 MSVC 2019+
- OpenSSL 3.x

### Linux

- Debian/Ubuntu 20.04+
- CMake 3.16+
- GCC 9+ 或 Clang 10+
- OpenSSL 3.x

## 快速开始

### 构建项目

```bash
# Windows (MinGW)
cmake -B build -G Ninja
cmake --build build --config Release

# Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

### 服务端示例

```cpp
#include "ipc/ipc_server.h"

int main() {
    tyke::IpcServer server;
    
    auto result = server.Start("my_service", 
        [](tyke::ClientId id, const std::vector<uint8_t>& data, 
           const tyke::ServerSendDataCallback& send_cb) -> std::optional<uint32_t> {
            // 处理客户端请求
            std::vector<uint8_t> response = {0x00, 0x01};
            send_cb(id, response);
            return 0;
        });
    
    if (!result.has_value()) {
        std::cerr << "Server start failed: " << result.error() << std::endl;
        return 1;
    }
    
    std::cout << "Server running. Press Enter to stop..." << std::endl;
    std::cin.get();
    server.Stop();
    return 0;
}
```

### 客户端示例

```cpp
#include "ipc/ipc_client.h"

int main() {
    tyke::IpcConnection conn;
    
    auto result = conn.Connect("my_service", 3000);
    if (!result.has_value()) {
        std::cerr << "Connect failed: " << result.error() << std::endl;
        return 1;
    }
    
    // 发送请求
    std::vector<uint8_t> request = {0xCA, 0xFE};
    conn.WriteEncrypted(request.data(), request.size(), 3000);
    
    // 接收响应
    conn.ReadLoop([](const std::vector<uint8_t>& data) -> bool {
        std::cout << "Received: " << data.size() << " bytes" << std::endl;
        return true;  // 返回 true 停止读取
    }, 3000);
    
    conn.Close();
    return 0;
}
```

### 使用静态方法

```cpp
#include "ipc/ipc_client.h"

int main() {
    std::vector<uint8_t> request = {0x01, 0x02};
    
    // 同步发送并接收响应
    auto result = tyke::IpcClient::Send("my_service", request,
        [](const std::vector<uint8_t>& data) -> bool {
            std::cout << "Response: " << data.size() << " bytes" << std::endl;
            return true;
        }, 3000);
    
    // 异步发送（不等待响应）
    tyke::IpcClient::SendAsync("my_service", request, 3000);
    
    return 0;
}
```

## API 文档

### IpcServer

| 方法 | 说明 |
|------|------|
| `Start(name, callback)` | 启动服务端监听 |
| `Stop()` | 停止服务端 |
| `SendToClient(id, data)` | 向指定客户端发送数据 |

### IpcConnection

| 方法 | 说明 |
|------|------|
| `Connect(name, timeout_ms)` | 连接到服务端 |
| `WriteEncrypted(data, size, timeout_ms)` | 发送加密数据 |
| `ReadLoop(callback, timeout_ms)` | 启动读取循环 |
| `Close()` | 关闭连接 |
| `IsValid()` | 检查连接有效性 |

### IpcClient (静态方法)

| 方法 | 说明 |
|------|------|
| `Send(name, request, callback, timeout_ms)` | 同步发送请求 |
| `SendAsync(name, request, timeout_ms)` | 异步发送请求 |

## 项目结构

```
tyke-cpp/
├── tyke/
│   ├── include/           # 头文件
│   │   ├── ipc/           # IPC 模块头文件
│   │   ├── core/          # 核心模块头文件
│   │   ├── component/     # 组件头文件
│   │   └── common/        # 通用定义
│   └── src/               # 源文件
│       ├── ipc/           # IPC 实现
│       ├── core/          # 核心实现
│       └── component/     # 组件实现
├── tests/                 # 测试程序
├── examples/              # 示例代码
├── third_party/           # 第三方依赖
└── docs/                  # 文档
```

## 测试

```bash
# 运行 IPC 测试
./build/tests/tyke_ipc_test

# 运行特定测试
./build/tests/tyke_ipc_test --functional
./build/tests/tyke_ipc_test --performance
```

## 性能指标

| 指标 | Windows | Linux |
|------|---------|-------|
| 大消息吞吐量 (16MB) | ~900 MB/s | ~800 MB/s |
| Ping-Pong 延迟 (P50) | ~26 μs | ~30 μs |
| 并发连接成功率 | 100% | 100% |

## 常见问题

### Q: 连接超时怎么办？

A: 检查服务端是否已启动，服务名称是否一致。Windows 上确保命名管道名称正确。

### Q: 大消息发送失败？

A: Tyke 支持 64KB 分片，最大消息 16MB。超过此限制需要应用层自行分片。

### Q: 如何实现跨语言通信？

A: Tyke 的 Go 版本使用相同的协议，可以与 C++ 版本互操作。参见跨语言测试文档。

## 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 联系方式

- 作者: Nick
- 日期: 2026-04-26
