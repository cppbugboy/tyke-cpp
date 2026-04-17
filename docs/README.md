# Tyke C++ IPC Framework

## 项目概述

Tyke 是一个高性能的跨平台 IPC（进程间通信）框架，提供安全、可靠的进程间通信能力。

### 核心特性

- **跨平台支持**：Windows（命名管道）和 Linux（Unix 域套接字）
- **安全通信**：ECDH 密钥交换 + AES-GCM 加密
- **高性能**：基于 IOCP/epoll 的异步 I/O
- **线程池**：内置线程池支持异步任务处理
- **协议封装**：28 字节协议头 + JSON 元数据 + 二进制内容

### 目录结构

```
cpp/
├── tyke/
│   ├── include/           # 头文件
│   │   ├── common/        # 通用定义
│   │   ├── core/          # 核心模块
│   │   ├── ipc/           # IPC 模块
│   │   └── component/     # 组件
│   └── src/               # 源文件
├── CMakeLists.txt         # CMake 配置
└── build.py               # 构建脚本
```

## 快速开始

### 构建项目

```bash
# Release 模式构建（默认）
python build.py

# Debug 模式构建
python build.py --debug

# 同时构建静态库和动态库
python build.py --all

# 清理构建产物
python build.py --clean
```

### 使用示例

```cpp
#include "core/tyke_framework.h"
#include "controller/controller.h"

using namespace tyke;

// 定义控制器
class MyController : public RequestController {
public:
    void RegisterMethod() override {
        RegisterRequestMethod("/hello", [](TykeRequest* req, TykeResponse* resp) {
            resp->SetContent(common::ContentType::TEXT, "Hello, World!");
            resp->Send();
        });
    }
};

int main() {
    // 注册控制器
    RegisterController(std::make_shared<MyController>());
    
    // 启动框架
    auto framework = TykeFramework::GetInstance();
    framework->SetThreadPoolCount(4)
             ->SetLogConfig("/var/log/tyke.log", "info", 10, 5);
    
    if (!framework->Start("my-server-uuid")) {
        return 1;
    }
    
    // 等待退出信号
    std::cin.get();
    
    framework->Stop();
    return 0;
}
```

## 构建指南

### 系统要求

- CMake 3.16+
- C++17 编译器
- OpenSSL 开发库

### Windows 构建

1. 安装 Visual Studio 2019+
2. 安装 CMake
3. 下载 OpenSSL 并配置 `third_party/lib/Windows`

```powershell
python build.py --release --all
```

### Linux 构建

1. 安装 GCC 7+ 或 Clang 6+
2. 安装 CMake 和 OpenSSL 开发包

```bash
sudo apt install cmake libssl-dev
python build.py --release --all
```

### 构建输出

- **静态库**：`build-release/lib/static/tyke.a` (Linux) 或 `tyke.lib` (Windows)
- **动态库**：`build-release/lib/shared/tyke.so` (Linux) 或 `tyke.dll` (Windows)

## API 参考

详细 API 文档请参阅 [API 参考](api-reference.md)。

## 常见问题

### Q: 如何处理粘包问题？

A: Tyke 使用帧协议，每个帧包含完整的消息。`FrameParser::ExtractFrame` 会自动处理帧边界。

### Q: 如何实现异步请求？

A: 使用 `TykeRequest::SendAsync` 或 `TykeRequest::SendAsyncWithFuture`。

### Q: 如何自定义日志输出？

A: 调用 `TykeFramework::SetLogConfig` 配置日志路径和级别。
