/**
 * @file ipc_types.h
 * @brief IPC模块类型定义
 * @author Nick
 * @date 2026/04/17
 *
 * 定义IPC模块使用的类型别名、常量和回调函数类型。
 */

#ifndef IPC_TYPES_H_
#define IPC_TYPES_H_

#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include "common/tyke_result.h"

namespace tyke
{
    /// 客户端标识类型
    using ClientId = uint64_t;

    /// IPC默认超时时间（毫秒）
    constexpr uint32_t kIpcDefaultTimeoutMs = 5000;

    /// IPC默认最大连接数
    constexpr size_t kIpcDefaultMaxConnections = 4;

    /// IPC默认空闲超时时间（毫秒）
    constexpr uint32_t kIpcDefaultIdleTimeoutMs = 60000;

    /// 客户端接收数据回调函数类型
    using ClientRecvDataCallback = std::function<bool(const std::vector<uint8_t> &)>;

    /// 服务器发送数据回调函数类型
    using ServerSendDataCallback = std::function<bool(ClientId, const std::vector<uint8_t> &)>;

    /// 服务器接收数据回调函数类型
    using ServerRecvDataCallback = std::function<nonstd::optional<uint32_t>(ClientId, const std::vector<uint8_t> &,
                                                 const ServerSendDataCallback&
    )
    >;
} // namespace tyke

#endif // IPC_TYPES_H_
