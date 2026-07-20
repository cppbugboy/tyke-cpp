/**
 * @file tyke_def.h
 * @brief Tyke框架核心协议与类型定义
 * @author Nick
 * @date 2026/04/16
 *
 * 定义IPC通信协议头结构、内容类型枚举、消息类型枚举等核心数据结构，
 * 以及协议魔数、缓冲区大小、超时时间等常量。
 */

#pragma once

#include <cstdint>
#include <nonstd/expected.hpp>
#include <string>
#include <unordered_map>

namespace tyke
{
    constexpr uint32_t kDefaultTimeoutMs = 5000;
    constexpr uint32_t kDefaultBufferSize = 4096;
    constexpr uint32_t kDefaultThreadPoolSize = 4;
    constexpr uint32_t kProtocolHeaderSize = 28;

    constexpr uint32_t kDefaultStubTimeoutMs = 30000;

    /**
     * @brief 状态码枚举，描述操作或请求的处理结果
     */
    enum class StatusCode
    {
        kNone = 0, ///< 无状态（初始值）
        kSuccess, ///< 操作成功
        kFailure, ///< 操作失败（通用错误）
        kTimeout, ///< 操作超时
        kMetadataError, ///< 元数据解析错误
        kContentError, ///< 内容数据错误
        kRouteError, ///< 路由未找到
        kModuleError, ///< 模块不支持
        kInternalError, ///< 内部错误
        kUnavailable, ///< 服务不可用
        kUnknownError, ///< 未知错误
    };

    /**
     * @brief 协议魔数，用于标识合法的Tyke协议数据包
     */
    constexpr char kProtocolMagic[4] = {'T', 'Y', 'K', 'E'};

    /**
     * @brief 内容类型枚举，定义请求/响应数据的编码格式
     */
    enum class ContentType
    {
        kText, ///< 纯文本
        kJson, ///< JSON格式
        kBinary, ///< 二进制数据
    };

    /**
     * @brief 内容类型到字符串的映射表
     */
    inline const std::unordered_map<ContentType, std::string>& ContentTypeMap()
    {
        static const std::unordered_map<ContentType, std::string> map = {
            {ContentType::kText, "text"},
            {ContentType::kJson, "json"},
            {ContentType::kBinary, "binary"},
        };
        return map;
    }

    template <typename T>
    using TResult = nonstd::expected<T, std::string>;

    using BoolResult = nonstd::expected<bool, std::string>;

    using ByteVecResult = nonstd::expected<std::vector<uint8_t>, std::string>;

    /**
     * @brief 消息类型枚举，区分同步/异步请求与响应的通信模式
     */
    enum class MessageType : uint32_t
    {
        kNone = 0, ///< 未指定类型
        kRequest = 1, ///< 同步请求
        kRequestAsync = 2, ///< 异步请求（回调模式）
        kRequestAsyncFunc = 3, ///< 异步请求（函数模式）
        kRequestAsyncFuture = 4, ///< 异步请求（Future模式）
        kResponse = 5, ///< 同步响应
        kResponseAsync = 6, ///< 异步响应（回调模式）
        kResponseAsyncFunc = 7, ///< 异步响应（函数模式）
        kResponseAsyncFuture = 8, ///< 异步响应（Future模式）
    };

    /**
     * @brief IPC通信协议头结构（28字节固定长度，按1字节对齐）
     *
     * 每个数据包的固定头部，包含协议标识、消息类型和负载长度信息。
     * 数据包格式: [ProtocolHeader][Metadata JSON][Content Binary]
     */
#pragma pack(push, 1)
    struct ProtocolHeader
    {
        char magic[4] = {'T', 'Y', 'K', 'E'}; ///< 协议魔数 "TYKE"，用于校验合法数据包
        MessageType msg_type = MessageType::kNone; ///< 消息类型（同步/异步请求或响应）
        uint32_t reserved[3] = {0}; ///< 保留字段（未来扩展用）
        uint32_t metadata_len = 0; ///< 元数据JSON段的字节长度
        uint32_t content_len = 0; ///< 内容二进制段的字节长度
    };
#pragma pack(pop)

    static_assert(sizeof(ProtocolHeader) == kProtocolHeaderSize,
                  "ProtocolHeader size mismatch with kProtocolHeaderSize");
} // namespace tyke
