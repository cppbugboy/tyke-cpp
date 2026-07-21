# Tyke C++

**Tyke** 是一个高性能、跨平台的 C++17 IPC（进程间通信）框架。它提供了完整的客户端-服务端通信层，内置路由、过滤器、对象池和异步消息机制，专为 Windows 和 Linux 上的本地进程通信而设计。

> 📘 Tyke 同时提供 [Go 语言实现](https://github.com/cppbugboy/tyke-go)，两者共享相同的传输协议，支持跨语言 IPC 通信。

---

## ✨ 特性

- **跨平台 IPC** — Windows（命名管道 + IOCP）和 Linux（Unix 域套接字 + epoll）
- **请求/响应模型** — 全双工消息传递，支持同步和异步模式
- **层级路由** — 树形路由分组，O(1) 哈希查找
- **过滤器链** — 可插拔的请求/响应前后拦截器
- **对象池** — 内置线程安全的对象池，用于 Request 和 Response，减少内存分配开销
- **优先级线程池** — 三级优先级调度（高/中/低），支持自动扩缩容
- **多级时间轮** — O(1) 定时器添加/移除，支持一次性定时器和周期任务
- **连接池** — 可复用的 IPC 客户端连接，支持空闲管理
- **大消息支持** — 超过 64 KB 的消息自动分片，最大支持 1 MB 载荷
- **高效协议** — 28 字节精简二进制头部，魔数标识（`TYKE`），小端序编码

---

## 📂 项目结构

```
tyke-cpp/
├── CMakeLists.txt              # 根构建配置
├── LICENSE                     # MIT 许可证
├── PROTOCOL.md                 # 传输协议规范
├── docs/
│   └── DESIGN.md               # 架构与设计文档
├── tyke/                       # 核心库
│   ├── CMakeLists.txt
│   ├── include/tyke/
│   │   ├── tyke.h              # 总入口头文件（包含所有公开接口）
│   │   ├── common/             # 类型定义、日志、工具函数
│   │   ├── core/               # 框架、路由器、分发器、过滤器
│   │   ├── component/          # 线程池、时间轮、对象池
│   │   └── ipc/                # 服务端、客户端、连接池、帧解析
│   └── src/tyke/               # 实现文件
├── examples/                   # 示例服务端与客户端
│   ├── server.cpp
│   ├── client.cpp
│   └── controllers/            # 示例请求/响应处理器
└── third_party/                # 仅头文件依赖
    └── include/
        ├── fmt/                # {fmt} 格式化库
        ├── spdlog/             # spdlog 日志库
        ├── nlohmann/           # JSON for Modern C++
        ├── nonstd/             # expected.hpp
        └── mimalloc*.h         # mimalloc 内存分配器
```

---

## 🚀 快速开始

### 环境要求

- **CMake** ≥ 3.16
- **C++17** 编译器（MSVC 2019+、GCC 8+、Clang 7+）
- **mimalloc** 运行时库（预编译文件位于 `third_party/lib/`）

### 构建

```bash
cd tyke-cpp

# 配置（静态库，Release 模式）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build --config Release

# 构建动态库
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build --config Release
```

构建选项：

| 选项                   | 默认值    | 说明                       |
| ---------------------- | --------- | -------------------------- |
| `BUILD_SHARED_LIBS`    | `OFF`     | 构建动态库                 |
| `BUILD_BOTH_LIBS`      | `OFF`     | 同时构建静态库和动态库     |
| `CMAKE_BUILD_TYPE`     | `Release` | 构建类型 (Debug / Release) |

### 运行示例

在一个终端启动服务端：

```bash
./build/bin/tyke_server_example
```

在另一个终端运行客户端：

```bash
./build/bin/tyke_client_example
```

客户端依次演示四种请求模式：同步请求、即发即弃异步、回调式异步、Future 异步。

---

## 💻 使用指南

### 1. 引入头文件

```cpp
#include "tyke/tyke.h"  // 一个头文件包含全部公开接口
```

### 2. 创建服务端

```cpp
#include "tyke/tyke.h"

int main() {
    auto& app = tyke::App();
    app.SetThreadPoolCount(4)
       .SetLogConfig("./server.log", "debug", 1024, 5);

    if (!app.Start("你的服务端UUID")) {
        return 1;
    }

    // 通过控制器注册请求处理器...

    // 等待关闭信号
    app.Shutdown();
    return 0;
}
```

### 3. 注册请求处理器

```cpp
#include "tyke/tyke.h"

class MyController : public tyke::ControllerBase {
public:
    void RegisterMethod() override {
        auto& router = tyke::Framework::GetRequestRouter();
        auto root = router.GetRoot();

        root->AddSubGroup("/api/user")
            ->AddRouteHandler("/login", [](const tyke::Request& req, tyke::Response& resp) {
                // 处理登录逻辑...
                resp.SetResult(tyke::StatusCode::kSuccess, "OK");
            });
    }
};

// 静态初始化时自动注册
REQUEST_CONTROLLER_REGISTER(my_ctrl, []() {
    MyController().RegisterMethod();
});
```

### 4. 发送请求（客户端）

**同步模式** — 阻塞直到收到响应：
```cpp
auto req = tyke::Request::Acquire();
req->SetModule("my_module");
req->SetRoute("/api/data/query");
req->SetContent(tyke::ContentType::kJson, json_bytes);

tyke::Response resp;
auto result = req->Send("服务端UUID", resp);
```

**回调式异步** — 通过回调函数接收响应：
```cpp
req->SendAsyncWithFunc("服务端UUID", [](const tyke::Response& resp) {
    // 处理异步响应...
});
```

**Future 异步** — 后续通过 `std::future` 阻塞获取：
```cpp
auto result = req->SendAsyncWithFuture("服务端UUID");
if (result) {
    auto response = result->get();  // 阻塞直到响应到达
}
```

**即发即弃** — 不处理响应：
```cpp
req->SendAsync("服务端UUID");
```

### 5. 添加过滤器

```cpp
class AuthFilter : public tyke::RequestFilter {
public:
    bool Before(const tyke::Request& req, tyke::Response& resp) override {
        // 验证令牌，返回 false 则拒绝请求
        return true;
    }
    bool After(const tyke::Request& req, tyke::Response& resp) override {
        // 后处理响应
        return true;
    }
};

// 挂载到路由分组
root->AddSubGroup("/api/admin")
    ->AddFilter(std::make_shared<AuthFilter>())
    ->AddRouteHandler("/dashboard", handler);
```

---

## 🧱 架构

```
┌─────────────────────────────────────────────────┐
│                    应用程序                       │
├─────────────────────────────────────────────────┤
│   控制器 (Controllers) │ 过滤器 (Filters) │ 路由器  │
├─────────────────────────────────────────────────┤
│   请求/响应  │  分发器 (Dispatcher)  │  存根管理   │
├─────────────────────────────────────────────────┤
│   IPC 服务端  │  IPC 客户端  │  连接池             │
├─────────────────────────────────────────────────┤
│   帧解析器  │  协议层 (28字节头部)                 │
├─────────────────────────────────────────────────┤
│   Windows 命名管道 (IOCP)  │  Linux Unix 域套接字  │
└─────────────────────────────────────────────────┘
```

| 组件           | 说明                                                                      |
| -------------- | ------------------------------------------------------------------------- |
| **Framework**  | 应用程序生命周期：初始化、启动、关闭。通过 `tyke::App()` 获取单例。          |
| **Router**     | 基于 CRTP 模式，O(1) 哈希查找。支持层级分组，过滤器链可继承。               |
| **Dispatcher** | 将收到的请求/响应通过过滤器链路由到注册的处理器。                            |
| **IPC 服务端**  | PIMPL 模式。Windows: IOCP + 命名管道。Linux: epoll + 抽象 Unix 套接字。    |
| **IPC 客户端**  | 静态工具类 (`IpcClient::Send`) 用于一次性请求；`IpcConnection` 用于持久连接。 |
| **连接池**     | 以服务端 UUID 为键的工厂管理池。复用连接，清理空闲连接。                    |
| **帧解析器**   | `[4B 长度][1B 类型][载荷]`。大于 64 KB 的载荷按 64 KB 分片。               |
| **线程池**     | 三级优先级队列（高 → 中 → 低），自动扩缩容，指标统计，异常恢复。            |
| **时间轮**     | 多级（4 层），O(1) 增删，支持一次性定时器和周期任务。                       |
| **对象池**     | 线程安全，`PooledPtr<T>` RAII 包装。Request (1024 上限) 和 Response (1024 上限)。 |

---

## 📦 依赖项

| 库                                                          | 版本    | 用途                     | 许可证   |
| ----------------------------------------------------------- | ------- | ------------------------ | -------- |
| [fmt](https://github.com/fmtlib/fmt)                        | ≥ 10.x  | 字符串格式化             | MIT      |
| [spdlog](https://github.com/gabime/spdlog)                  | ≥ 1.x   | 日志（调试/信息/警告/错误） | MIT      |
| [nlohmann/json](https://github.com/nlohmann/json)           | ≥ 3.x   | JSON 解析                | MIT      |
| [mimalloc](https://github.com/microsoft/mimalloc)           | ≥ 2.x   | 内存分配器               | MIT      |
| [nonstd/expected](https://github.com/martinmoene/expected-lite) | ≥ 0.x | 结果类型 (`TResult<T>`) | BSL 1.0  |

除 mimalloc 需要链接预编译库外，其余依赖均为头文件形式（位于 `third_party/include/`）。

---

## 🔌 协议

传输协议详见 `PROTOCOL.md`。核心要点：

- **魔数**: `TYKE`（4 字节）
- **头部**: 28 字节固定长度，1 字节对齐
- **消息类型**: 请求、响应及各类异步变体（回调、Future）
- **内容类型**: 文本、JSON、二进制
- **帧格式**: `[4B 总长度 (小端序)][1B 类型][载荷]`
- **分片**: 超过 64 KB 的消息按 64 KB 分片，每片带 8 字节分片头
- **最大载荷**: 每帧 1 MB

该协议与 [Go 实现](https://github.com/cppbugboy/tyke-go) 共享，支持跨语言 IPC 通信。

---

## 📄 许可证

MIT 许可证。详见 [LICENSE](LICENSE)。
