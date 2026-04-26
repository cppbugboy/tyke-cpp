# Tyke C++ 设计文档

## 1. 概述

Tyke 是一个高性能、跨平台的本地进程间通信（IPC）框架，提供安全加密的双向通信能力。本文档详细阐述 C++ 实现的架构设计、核心数据结构和关键算法。

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
│  │              Crypto (ECDH + AES-GCM)                    ││
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
│ total_len│ frame_type  │ payload                           │
│ (LE)     │             │                                   │
├──────────┼─────────────┼───────────────────────────────────┤
│ 帧类型   │ 值          │ 说明                              │
├──────────┼─────────────┼───────────────────────────────────┤
│ HandshakeInit │ 0x01   │ 客户端ECDH公钥 (SPKI DER)         │
│ HandshakeResp │ 0x02   │ 服务端ECDH公钥 (SPKI DER)         │
│ Data          │ 0x03   │ AES-GCM加密数据                   │
│ DataFragment  │ 0x04   │ 分片数据 (大消息)                 │
└──────────┴─────────────┴───────────────────────────────────┘
```

### 3.2 分片帧载荷

```
┌────────────────────────────────────────────────────────────┐
│ Fragment Payload Format                                     │
├──────────┬──────────┬───────────────────────────────────────┤
│ 4 bytes  │ 4 bytes  │ N bytes                               │
│ total_sz │ offset   │ encrypted_chunk                       │
│ (LE)     │ (LE)     │                                       │
└──────────┴──────────┴───────────────────────────────────────┘
```

### 3.3 加密数据格式

```
┌────────────────────────────────────────────────────────────┐
│ Encrypted Data Format                                       │
├──────────┬─────────────────┬──────────────┐                │
│ 12 bytes │ N bytes         │ 16 bytes     │                │
│ IV       │ Ciphertext      │ Auth Tag     │                │
└──────────┴─────────────────┴──────────────┘                │

IV 结构: [4B 随机前缀][8B 大端计数器]
```

### 3.4 应用层协议头

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

### 4.1 ECDH 密钥交换

```cpp
// 1. 双方各自生成ECDH密钥对 (P-256)
EcdhKeyExchange alice, bob;
alice.GenerateKey();  // OpenSSL EVP_PKEY_Q_keygen
bob.GenerateKey();

// 2. 交换公钥 (X.509 SPKI DER格式)
auto alice_pub = alice.GetPublicKeyDer();  // i2d_PUBKEY
auto bob_pub = bob.GetPublicKeyDer();

// 3. 计算共享密钥
auto alice_secret = alice.ComputeSharedSecret(bob_pub);  // EVP_PKEY_derive
auto bob_secret = bob.ComputeSharedSecret(alice_pub);
// alice_secret == bob_secret (32字节)
```

### 4.2 HKDF 密钥派生

```cpp
// 从ECDH共享密钥派生AES-256密钥
// HKDF-SHA256(salt="tyke-v1-hkdf-salt", info="tyke-v1-aes256-key")
key = HKDF-Extract(salt, shared_secret)  // PRK = HMAC-SHA256(salt, IKM)
key = HKDF-Expand(PRK, info, 32)         // OKM = HMAC-SHA256(PRK, info || counter)
```

### 4.3 AES-GCM 加密

```cpp
// IV生成: [4B随机][8B大端计数器]
// 每次加密递增计数器，确保IV唯一性
std::vector<uint8_t> iv(12);
RAND_bytes(iv.data(), 4);  // 随机前缀
counter = be64(counter + 1);  // 大端计数器

// AES-GCM加密
EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), ...);
EVP_EncryptUpdate(ctx, out, &len, in, in_len);
EVP_EncryptFinal_ex(ctx, out + len, &len);
EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
```

### 4.4 大消息分片

```cpp
// 分片参数
constexpr uint32_t kFragmentChunkSize = 64 * 1024;  // 64KB

// 发送端分片
for (offset = 0; offset < total_size; offset += chunk_size) {
    chunk_size = min(remaining, kFragmentChunkSize);
    encrypted_chunk = AES_GCM_Encrypt(data + offset, chunk_size);
    fragment_payload = EncodeLe32(total_size) + EncodeLe32(offset) + encrypted_chunk;
    SendFrame(kMsgDataFragment, fragment_payload);
}

// 接收端重组
if (offset == 0) {
    reassembly_buffer.resize(total_size);
}
memcpy(reassembly_buffer.data() + offset, decrypted_chunk.data(), decrypted_chunk.size());
received += decrypted_chunk.size();
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
│  - WriteEncrypted │  - Invoke user callback                 │
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

### 8.3 加密优化

- 复用 EVP_CTX 上下文，避免重复初始化
- 批量加密减少函数调用开销
- IV 计数器避免随机数生成

## 9. 安全考虑

### 9.1 密钥安全

- ECDH 使用 P-256 曲线，提供 128 位安全强度
- 密钥材料使用 `OPENSSL_cleanse` 安全清零
- 每个连接使用独立的会话密钥

### 9.2 通信安全

- AES-GCM 提供认证加密，防止篡改
- IV 唯一性保证，防止重放攻击
- 帧长度限制 (16MB)，防止内存耗尽

### 9.3 平台安全

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
    virtual BoolResult WriteEncrypted(const void* data, size_t size, uint32_t timeout_ms) = 0;
    virtual BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms) = 0;
    virtual void Close() = 0;
    virtual bool IsValid() const = 0;
};
```

### 11.2 新加密算法

修改 `ipc_crypto.cpp` 中的密钥派生和加密逻辑。

## 12. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-04-26 | 初始版本，支持 Windows/Linux 跨平台 IPC |
