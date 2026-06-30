# Tyke C++ 设计文档

## 1. 概述

Tyke 是一个高性能、跨平台的本地进程间通信（IPC）框架，提供高性能的双向通信能力。本文档详细阐述 C++ 实现的架构设计、核心数据结构和关键算法。

## 2. 系统架构

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
├─────────────────────────────────────────────────────────────┤
│  Request/Response  │  Controller  │  Dispatcher  │  Router  │
├─────────────────────────────────────────────────────────────┤
│                        IPC Layer                             │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐ │
│  │ IpcServer   │  │ IpcConnection│  │ ConnectionPool      │ │
│  └─────────────┘  └──────────────┘  └─────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────┐│
│  │              Frame Parser (Build/Extract Frame)         ││
│  └─────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│                    Platform Layer                            │
│  ┌─────────────────────┐  ┌─────────────────────────────┐   │
│  │ Windows (IOCP)      │  │ Linux (epoll)               │   │
│  │ Named Pipe          │  │ Unix Domain Socket          │   │
│  └─────────────────────┘  └─────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 模块划分

| 模块 | 目录 | 职责 |
|------|------|------|
| **IPC** | `tyke/ipc/` | 进程间通信核心功能 |
| **Core** | `tyke/core/` | 请求/响应处理、路由分发 |
| **Component** | `tyke/component/` | 线程池、对象池、时间轮 |
| **Common** | `tyke/common/` | 通用定义、工具函数、日志 |
| **Platform** | `tyke/ipc/` (platform_*.cpp) | 平台相关实现 |

## 3. 核心数据结构

### 3.1 帧协议

```
┌────────────────────────────────────────────────────────────┐
│ Frame Format                                               │
├──────────┬─────────────┬───────────────────────────────────┤
│ 4 bytes  │ 1 byte      │ N bytes                           │
│ total_len│ frame_type  │ payload (plaintext)               │
│ (LE)     │             │                                   │
├──────────┼─────────────┼───────────────────────────────────┤
│ 帧类型   │ 值          │ 说明                              │
├──────────┼─────────────┼───────────────────────────────────┤
│ Data          │ 0x03   │ 明文数据                          │
│ DataFragment  │ 0x04   │ 分片数据 (大消息)                 │
└──────────┴─────────────┴───────────────────────────────────┘
```

### 3.2 分片帧载荷

```
┌────────────────────────────────────────────────────────────┐
│ Fragment Payload Format                                     │
├──────────┬──────────┬───────────────────────────────────────┤
│ 4 bytes  │ 4 bytes  │ N bytes                               │
│ total_sz │ offset   │ chunk                                 │
│ (LE)     │ (LE)     │ (plaintext)                           │
└──────────┴──────────┴───────────────────────────────────────┘
```

### 3.3 应用层协议头

```
┌────────────────────────────────────────────────────────────┐
│ Protocol Header (28 bytes)                                  │
├──────────┬──────────┬──────────────┬──────────┬───────────┤
│ 4 bytes  │ 4 bytes  │ 12 bytes     │ 4 bytes  │ 4 bytes   │
│ magic    │ msg_type │ reserved     │ meta_len │ cont_len  │
│ "TYKE"   │ (LE)     │ (zeros)      │ (LE)     │ (LE)      │
└──────────┴──────────┴──────────────┴──────────┴───────────┘
```

## 4. 关键算法

### 4.1 大消息分片

```cpp
// 分片参数
constexpr uint32_t kFragmentChunkSize = 64 * 1024;  // 64KB

// 发送端分片
for (offset = 0; offset < total_size; offset += chunk_size) {
    chunk_size = min(remaining, kFragmentChunkSize);
    chunk = data + offset;  // 明文 chunk
    fragment_payload = EncodeLe32(total_size) + EncodeLe32(offset) + chunk;
    SendFrame(kMsgDataFragment, fragment_payload);
}

// 接收端重组
if (offset == 0) {
    reassembly_buffer.resize(total_size);
}
memcpy(reassembly_buffer.data() + offset, chunk.data(), chunk.size());
received += chunk.size();
if (received == total_size) {
    // 完整消息已接收
    OnCompleteMessage(reassembly_buffer);
}
```

## 5. 平台实现

### 5.1 Windows (IOCP)

```cpp
// 服务端架构
CreateNamedPipeA("\\\\.\\pipe\\<name>", 
                 PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                 PIPE_TYPE_BYTE | PIPE_WAIT,
                 PIPE_UNLIMITED_INSTANCES,
                 262144,  // 输出缓冲区 256KB
                 262144,  // 输入缓冲区 256KB
                 ...);

// IOCP工作线程
HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
for (int i = 0; i < num_workers; i++) {
    workers.emplace_back(WorkerProc);
}

// 异步读取
ReadFile(pipe, buffer, size, &bytes_read, &overlapped);
GetQueuedCompletionStatus(iocp, &bytes_transferred, &key, &overlapped, INFINITE);
```

### 5.2 Linux (epoll + Unix Domain Socket)

```cpp
// 服务端架构 (abstract namespace)
struct sockaddr_un addr;
addr.sun_family = AF_UNIX;
addr.sun_path[0] = '\0';  // abstract namespace
strcpy(addr.sun_path + 1, "tyke_<name>");

int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
listen(server_fd, SOMAXCONN);

// epoll事件循环
int epfd = epoll_create1(0);
epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);
epoll_wait(epfd, events, MAX_EVENTS, -1);
```

## 6. 连接池设计

```cpp
class ConnectionPool {
    std::queue<std::shared_ptr<IpcConnection>> idle_;
    std::atomic<int32_t> active_;
    std::mutex mu_;
    std::condition_variable cv_;
    
    std::shared_ptr<IpcConnection> Acquire() {
        // 1. 尝试从idle队列获取
        // 2. 若idle为空且active < max，创建新连接
        // 3. 否则等待其他连接释放
    }
    
    void Release(std::shared_ptr<IpcConnection> conn, bool unhealthy) {
        // 若连接健康，放回idle队列
        // 否则关闭连接，减少active计数
    }
};
```

## 7. 线程模型

### 7.1 服务端线程模型

```
┌─────────────────────────────────────────────────────────────┐
│                      Server Threads                          │
├─────────────────────────────────────────────────────────────┤
│  Accept Thread    │  IOCP/epoll Worker Threads              │
│  - Listen for     │  - Handle client I/O                    │
│    connections    │  - Process frames                       │
│                   │  - Dispatch to thread pool              │
├───────────────────┼─────────────────────────────────────────┤
│  Thread Pool      │  User Callbacks                         │
│  - Process        │  - Business logic                       │
│    requests       │  - Send responses                       │
└───────────────────┴─────────────────────────────────────────┘
```

### 7.2 客户端线程模型

```
┌─────────────────────────────────────────────────────────────┐
│                      Client Threads                          │
├─────────────────────────────────────────────────────────────┤
│  Main Thread      │  Read Thread (optional)                 │
│  - Connect        │  - ReadLoop for async receive           │
│  - Write          │  - Invoke user callback                 │
│  - Close          │                                         │
└───────────────────┴─────────────────────────────────────────┘
```

## 8. 性能优化

### 8.1 内存管理

- 使用 `mimalloc` 替代系统分配器，减少内存碎片
- 对象池复用 Request/Response 对象
- 连接池复用 IPC 连接

### 8.2 I/O 优化

- Windows: IOCP 异步 I/O，避免阻塞
- Linux: epoll 边缘触发模式
- 增大管道/套接字缓冲区至 256KB

## 9. 安全考虑

### 9.1 通信安全

- 明文传输，适用于可信本地 IPC 场景
- 帧长度限制 (16MB)，防止内存耗尽

### 9.2 平台安全

- Windows: 命名管道默认安全描述符
- Linux: abstract namespace 避免文件系统权限问题

## 10. 错误处理

### 10.1 Result 类型

```cpp
template<typename T>
class Result {
    std::optional<T> value_;
    std::string error_;
public:
    bool HasValue() const { return value_.has_value(); }
    T& Value() { return value_.value(); }
    const std::string& Err() const { return error_; }
};

using BoolResult = Result<bool>;
using ByteVecResult = Result<std::vector<uint8_t>>;
```

### 10.2 错误传播

- 所有可能失败的操作返回 `Result` 类型
- 错误信息包含上下文，便于调试
- 日志记录关键错误

## 11. 扩展性

### 11.1 新平台支持

实现 `IClientConnectionImpl` 和 `IServerImpl` 接口：

```cpp
class IClientConnectionImpl {
public:
    virtual BoolResult Connect(std::string_view server_name, uint32_t timeout_ms) = 0;
    virtual BoolResult Write(const void* data, size_t size, uint32_t timeout_ms) = 0;
    virtual BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms) = 0;
    virtual void Close() = 0;
    virtual bool IsValid() const = 0;
};
```

### 11.2 帧解析扩展

修改 `ipc_frame.cpp` / `ipc_frame.h` 中的帧构建与解析逻辑（如需新增帧类型或调整分片策略）。

## 12. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-04-26 | 初始版本，支持 Windows/Linux 跨平台 IPC |
| 1.1 | 2026-06-29 | 移除加密层（ECDH/AES-GCM/OpenSSL），数据改为明文传输；删除握手帧；`WriteEncrypted` 重命名为 `Write`；`ipc_crypto.*` 替换为 `ipc_frame.*` |
