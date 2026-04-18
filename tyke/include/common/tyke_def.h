/**
 * @file tyke_def.h
 * @brief Tyke框架核心协议与类型定义
 * @author Nick
 * @date 2026/04/16
 *
 * 定义IPC通信协议头结构、内容类型枚举、消息类型枚举等核心数据结构，
 * 以及协议魔数、缓冲区大小、超时时间等常量。
 */

#ifndef TYKE_DEF_H
#define TYKE_DEF_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace tyke
{
constexpr uint32_t kDefaultTimeoutMs      = 5000;
constexpr uint32_t kDefaultBufferSize     = 4096;
constexpr uint32_t kDefaultThreadPoolSize = 4;
constexpr uint32_t kProtocolHeaderSize    = 28;

constexpr uint32_t kAesGcmIvLen           = 12;
constexpr uint32_t kAesGcmTagLen          = 16;
constexpr uint32_t kAes256KeyLen          = 32;
constexpr uint32_t kDefaultStubTimeoutMs  = 30000;
constexpr int      kHttpStatusNotFound    = 404;
constexpr int      kHttpStatusTimeout     = 408;

/**
 * @brief 协议魔数，用于标识合法的Tyke协议数据包
 */
constexpr char kProtocolMagic[4] = {'T', 'Y', 'K', 'E'};

/**
 * @brief 内容类型枚举，定义支持的数据编码格式
 */
enum class ContentType
{
    kText,
    kJson,
    kBinary,
};

/**
 * @brief 内容类型到字符串的映射表
 */
inline const std::unordered_map<ContentType, std::string> &ContentTypeMap()
{
    static const std::unordered_map<ContentType, std::string> map = {
            {ContentType::kText, "text"},
            {ContentType::kJson, "json"},
            {ContentType::kBinary, "binary"},
    };
    return map;
}

/**
 * @brief 消息类型枚举，区分同步/异步请求与响应
 */
enum class MessageType
{
    kNone = 0,
    kRequest = 1,
    kRequestAsync = 2,
    kRequestAsyncFunc = 3,
    kRequestAsyncFuture = 4,
    kResponse = 5,
    kResponseAsync = 6,
    kResponseAsyncFunc = 7,
    kResponseAsyncFuture = 8,
};

/**
 * @brief IPC通信协议头结构
 *
 * 每个数据包的固定头部，包含协议标识、消息类型和负载长度信息。
 * 数据包格式: [ProtocolHeader][Metadata JSON][Content Binary]
 */
struct ProtocolHeader
{
    char        magic[4] = {'T', 'Y', 'K', 'E'};
    MessageType msg_type;
    uint32_t    reserved[3]  = {0};
    uint32_t    metadata_len = 0;
    uint32_t    content_len  = 0;
};

}

#endif //TYKE_DEF_H