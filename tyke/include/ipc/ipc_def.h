/**
 * @file ipc_types.h
 * @brief IPC模块类型定义
 * @author Nick
 * @date 2026/04/17
 *
 * 定义IPC模块使用的类型别名、常量和回调函数类型。
 */

#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <optional>

namespace tyke
{
    /// 客户端标识类型
    using ClientId = uint64_t;

    /// IPC默认超时时间（毫秒）
    constexpr uint32_t kIpcDefaultTimeoutMs = 5000;

    /// IPC默认最大连接数
    constexpr size_t kIpcDefaultMaxConnections = 4;

    /// 客户端接收数据回调函数类型
    using ClientRecvDataCallback = std::function<bool(const std::vector<unsigned char>&)>;

    /// 服务器发送数据回调函数类型
    using ServerSendDataCallback = std::function<bool(ClientId, const std::vector<unsigned char>&)>;

    /**
     * @brief 服务器接收数据回调函数类型
     * @param ClientId 客户端标识
     * @param std::vector<uint8_t> 接收到的原始数据
     * @param ServerSendDataCallback 发送数据的回调函数
     * @return std::optional<uint32_t> - 有值时表示已消费字节数（0表示数据不合法已丢弃），
     *         无值(std::nullopt)表示数据不完整需等待更多数据
     */
    using ServerRecvDataCallback = std::function<std::optional<uint32_t>(
            ClientId, const std::vector<unsigned char>&,
            const ServerSendDataCallback&
        )
    >;
} // namespace tyke
