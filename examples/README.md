# Tyke 示例代码说明

本目录包含Tyke框架的示例代码，演示同步和异步请求的使用方法。

## 目录结构

```
examples/
├── CMakeLists.txt              # C++示例编译配置
├── server.cpp                  # 服务端示例
├── client.cpp                  # 原有客户端示例
├── sync_client.cpp             # C++同步请求客户端示例
├── async_client.cpp            # C++异步请求客户端示例
├── go_examples/                # Golang示例代码
│   ├── sync_client/main.go     # Go同步请求客户端示例
│   └── async_client/main.go    # Go异步请求客户端示例
├── test_request_controller.*   # 测试请求控制器
├── test_request_filter.*       # 测试请求过滤器
├── test_response_controller.*  # 测试响应控制器
└── test_response_filter.*      # 测试响应过滤器
```

## 同步请求

同步请求会阻塞当前线程/goroutine，直到收到服务端的响应或超时。

### 工作流程

```
客户端                                服务端
  │                                    │
  │  1. 发送请求                        │
  │ ─────────────────────────────────► │
  │                                    │  2. 处理请求
  │                                    │
  │  3. 返回响应                        │
  │ ◄───────────────────────────────── │
  │  4. 处理响应                        │
  │                                    │
```

### 使用示例

**C++:**
```cpp
auto request = tyke::MakeRequestPtr();
auto response = tyke::MakeResponsePtr();

request->SetModule("test")
       .SetRoute("/test/hello")
       .SetContent(tyke::ContentType::kText, content);

auto result = request->Send(SERVER_UUID, *response);
if (result.has_value()) {
    // 处理响应
}
```

**Go:**
```go
request := core.AcquireRequest()
defer core.ReleaseRequest(request)
response := core.AcquireResponse()
defer core.ReleaseResponse(response)

request.SetModule("test").SetRoute("/test/hello").SetContent(common.ContentTypeText, content)

result := request.Send(ServerUUID, response)
if result.HasValue() {
    // 处理响应
}
```

## 异步请求

异步请求发送后立即返回，响应通过不同的机制处理。

### 核心机制

```
客户端                                服务端
  │                                    │
  │  1. 启动监听IPC Server             │
  │     (绑定listen_uuid)              │
  │                                    │
  │  2. 发送异步请求                    │
  │     (SetAsyncUuid设置async_uuid)   │
  │ ─────────────────────────────────► │
  │                                    │  3. 处理请求
  │                                    │
  │                    4. 发送响应到async_uuid
  │ ◄───────────────────────────────── │
  │  5. 监听IPC Server接收响应          │
  │  6. 根据消息类型处理响应             │
  │                                    │
```

### 三种异步请求方式

| 方式 | 消息类型 | 响应处理机制 | 使用场景 |
|------|----------|--------------|----------|
| `SendAsync` | kRequestAsync → kResponseAsync | ResponseRouter路由分发 | 需要统一响应处理逻辑 |
| `SendAsyncWithFunc` | kRequestAsyncFunc → kResponseAsyncFunc | 执行回调函数 | 简单场景，直接处理响应 |
| `SendAsyncWithFuture` | kRequestAsyncFuture → kResponseAsyncFuture | Future/Promise结果 | 需要同步等待异步结果 |

### 方式一：SendAsync

通过ResponseRouter注册响应处理器，响应到达时自动路由分发。

**C++:**
```cpp
// 注册响应处理器
auto root_group = tyke::RESPONSE_ROUTER_INSTANCE->GetRoot();
root_group->AddRouteHandler("/test/hello", [](const tyke::TykeResponse& response) {
    // 处理响应
});

// 发送异步请求
request->SetAsyncUuid(listen_uuid);
request->SendAsync(SERVER_UUID, listen_uuid);
```

**Go:**
```go
// 注册响应处理器
rootGroup := core.GetResponseRouterInstance().GetRoot()
rootGroup.AddRouteHandler("/test/hello", func(response *core.TykeResponse) {
    // 处理响应
})

// 发送异步请求
request.SetAsyncUuid(listenUuid)
request.SendAsync(ServerUUID, listenUuid)
```

### 方式二：SendAsyncWithFunc

发送请求时注册回调函数，响应到达时自动调用。

**C++:**
```cpp
request->SetAsyncUuid(listen_uuid);
request->SendAsyncWithFunc(SERVER_UUID, [](const tyke::TykeResponse& response) {
    // 处理响应
});
```

**Go:**
```go
request.SetAsyncUuid(listenUuid)
request.SendAsyncWithFunc(ServerUUID, func(response *core.TykeResponse) {
    // 处理响应
})
```

### 方式三：SendAsyncWithFuture

返回Future对象，可在需要时阻塞等待响应。

**C++:**
```cpp
request->SetAsyncUuid(listen_uuid);
auto future = request->SendAsyncWithFuture(SERVER_UUID, listen_uuid);
if (future.has_value()) {
    tyke::TykeResponse response = future.value().GetResponse();
    // 处理响应
}
```

**Go:**
```go
request.SetAsyncUuid(listenUuid)
future, err := request.SendAsyncWithFuture(ServerUUID, listenUuid)
if err == nil {
    response := future.GetResponse()
    // 处理响应
}
```

## 编译和运行

### C++ 示例

**编译：**
```bash
cd tyke-cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**运行：**
```bash
# 启动服务端
./build/bin/server.exe

# 运行同步客户端
./build/bin/sync_client.exe

# 运行异步客户端
./build/bin/async_client.exe
```

### Golang 示例

**部署示例代码：**
```bash
# 将go_examples目录下的示例复制到tyke-go项目
cp -r examples/go_examples/sync_client ../tyke-go/examples/
cp -r examples/go_examples/async_client ../tyke-go/examples/
```

**编译和运行：**
```bash
cd tyke-go

# 编译
go build ./examples/sync_client
go build ./examples/async_client

# 运行
./sync_client.exe
./async_client.exe
```

## 关键注意事项

1. **async_uuid的正确使用**：
   - 客户端启动的监听服务器UUID即为`async_uuid`
   - 使用`SetAsyncUuid()`方法设置到请求中
   - 服务端根据此UUID将响应发送到正确的客户端

2. **响应路由注册**：
   - `SendAsync`方式需要在客户端注册响应路由处理器
   - 路由路径应与服务端处理的请求路径对应

3. **资源清理**：
   - 确保在程序退出前停止监听服务器
   - 释放对象池中的请求和响应对象

4. **服务端UUID**：
   - 示例中使用固定的服务端UUID：`39649d81-81c5-4f6e-b6a9-e768b55063be`
   - 实际使用时需要与服务端配置一致
