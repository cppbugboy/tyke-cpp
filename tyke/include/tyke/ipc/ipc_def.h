/**
 * @file ipc_def.h
 * @brief IPC模块类型定义。声明IPC通信所需的类型别名、常量和回调函数签名。
 * @author Nick
 * @date 2026/04/17
 *
 * @details
 * 本文件定义IPC模块使用的核心类型，包括：
 * - ClientId: 客户端连接的唯一标识符
 * - 超时与容量常量：连接超时、最大连接数、空闲超时
 * - 回调函数类型：客户端与服务端的数据收发回调签名
 */

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace tyke
{
    /// @brief 客户端连接的唯一标识符，由服务端在客户端连接时分配
    using ClientId = uint64_t;

    /// @brief IPC通信的默认超时时间（毫秒），适用于连接建立和读写操作
    constexpr uint32_t kIpcDefaultTimeoutMs = 5000;

    /// @brief 连接池默认最大连接数，限制同时打开的IPC连接数量
    constexpr size_t kIpcDefaultMaxConnections = 4;

    /// @brief 连接池默认空闲超时时间（毫秒），空闲超过此时间的连接将被回收
    constexpr uint32_t kIpcDefaultIdleTimeoutMs = 30000;

    /// @brief 客户端接收数据回调函数类型
    /// @param std::vector<uint8_t> 接收到的数据（分片消息已自动重组）
    /// @return true 停止接收；false 继续等待下一条消息
    using ClientRecvDataCallback = std::function<bool(const std::vector<uint8_t> &)>;

    /// @brief 服务端向指定客户端发送数据的回调函数类型
    /// @param ClientId 目标客户端标识
    /// @param std::vector<uint8_t> 要发送的数据
    /// @return true 发送成功；false 发送失败
    using ServerSendDataCallback = std::function<bool(ClientId, const std::vector<uint8_t> &)>;

    /**
     * @brief 服务器接收数据回调函数类型
     * @param ClientId 客户端标识
     * @param std::vector<uint8_t> 接收到的原始数据
     * @param ServerSendDataCallback 发送数据的回调函数
     * @return std::optional<uint32_t> - 有值时表示已消费字节数（0表示数据不合法已丢弃），
     *         无值(std::nullopt)表示数据不完整需等待更多数据
     */
    using ServerRecvDataCallback =
    std::function<std::optional<uint32_t>(ClientId, const std::vector<uint8_t> &, const ServerSendDataCallback &)>;
} // namespace tyke