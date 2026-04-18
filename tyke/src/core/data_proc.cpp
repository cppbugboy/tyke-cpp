/**
 * @file data_proc.cpp
 * @brief 数据编解码处理器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现DataProc类的具体逻辑，提供请求和响应的序列化与反序列化功能。
 * 协议格式: [ProtocolHeader(固定28字节)][Metadata JSON][Content Binary]
 */

#include "core/data_proc.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "common/log_def.h"

namespace tyke
{
/**
     * @brief 通用编码模板函数
     * @tparam T 请求或响应类型
     * @param msg 待编码的消息对象
     * @param data_vec 输出的字节向量
     * @return 成功返回true，失败返回错误信息
     *
     * 实现请求和响应的通用编码逻辑，包括元数据JSON序列化和协议头组装。
     */
template<typename T>
void DataProc::Encode(T &msg, std::vector<unsigned char> &data_vec)
{
    try
    {
        // 序列化元数据为JSON字符串
        std::string metadata_string;
        msg.metadata_.ToJsonString(metadata_string);

        // 计算各部分大小
        const size_t header_size  = sizeof(ProtocolHeader);
        const size_t meta_size    = metadata_string.size();
        const size_t content_size = msg.content_.size();
        const size_t total_size   = header_size + meta_size + content_size;

        // 预分配空间
        data_vec.clear();
        data_vec.resize(total_size);

        // 填充协议头
        msg.protocol_header_.metadata_len = static_cast<uint32_t>(meta_size);
        msg.protocol_header_.content_len  = static_cast<uint32_t>(content_size);

        unsigned char *ptr = data_vec.data();

        // 复制协议头
        std::memcpy(ptr, &msg.protocol_header_, header_size);
        ptr += header_size;

        // 复制元数据
        if (meta_size > 0)
        {
            std::memcpy(ptr, metadata_string.data(), meta_size);
            ptr += meta_size;
        }

        // 复制内容数据
        if (content_size > 0)
        {
            std::memcpy(ptr, msg.content_.data(), content_size);
        }

        LOG_DEBUG("Encode completed: header={} bytes, metadata={} bytes, content={} bytes, total={} bytes", header_size,
                  meta_size, content_size, total_size);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(e.what());
    }
}

/**
     * @brief 通用解码模板函数
     * @tparam T 请求或响应类型
     * @param data_vec 待解码的字节向量
     * @param msg 输出的消息对象
     * @param data_size 解码的数据大小
     * @return 成功返回true，失败返回错误信息
     *
     * 实现请求和响应的通用解码逻辑，包括协议头解析和元数据JSON反序列化。
     */
template<typename T>
nonstd::optional<bool> DataProc::Decode(const std::vector<unsigned char> &data_vec, T &msg, uint32_t &data_size)
{
    try
    {
        data_size                = 0;
        const size_t vec_size    = data_vec.size();
        const size_t header_size = sizeof(ProtocolHeader);

        // 检查数据长度是否足够包含协议头
        if (vec_size < header_size)
        {
            LOG_ERROR("Data too short for header: expected {} bytes, got {}", header_size, vec_size);
            return false;
        }

        // 解析协议头
        std::memcpy(&msg.protocol_header_, data_vec.data(), header_size);

        // 验证协议魔数
        if (std::memcmp(msg.protocol_header_.magic, kProtocolMagic, sizeof(msg.protocol_header_.magic)) != 0)
        {
            LOG_ERROR("Protocol magic mismatch: expected TYKE");
            throw std::runtime_error("Protocol magic mismatch");
        }

        const uint32_t meta_len = msg.protocol_header_.metadata_len;
        const uint32_t cont_len = msg.protocol_header_.content_len;

        // 检查数据完整性
        if (vec_size < header_size + meta_len + cont_len)
        {
            LOG_ERROR("Data incomplete: expected {} bytes, got {}", header_size + meta_len + cont_len, vec_size);
            return false;
        }

        // 解析元数据
        if (meta_len > 0)
        {
            std::string meta_str(reinterpret_cast<const char *>(data_vec.data() + header_size), meta_len);
            msg.metadata_.FromJsonString(meta_str);
        }

        // 复制内容数据
        const unsigned char *content_start = data_vec.data() + header_size + meta_len;
        msg.content_.assign(content_start, content_start + cont_len);

        data_size = static_cast<uint32_t>(header_size + meta_len + cont_len);
        LOG_DEBUG("Decode completed: header={} bytes, metadata={} bytes, content={} bytes, total={} bytes", header_size,
                  meta_len, cont_len, data_size);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(e.what());
    }

    return true;
}

/**
     * @brief 编码请求对象为协议格式
     * @param request 待编码的请求对象
     * @param data_vec 输出的字节向量
     * @return 成功返回true，失败返回错误信息
     */
void DataProc::EncodeRequest(TykeRequest &request, std::vector<unsigned char> &data_vec)
{
    try
    {
        Encode(request, data_vec);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(e.what());
    }
}

/**
     * @brief 解码协议格式为请求对象
     * @param data_vec 待解码的字节向量
     * @param request 输出的请求对象
     * @param data_size 解码的数据大小
     * @return 成功返回true，失败返回错误信息
     */
nonstd::optional<bool> DataProc::DecodeRequest(const std::vector<unsigned char> &data_vec, TykeRequest &request,
                                   uint32_t &data_size)
{
    try
    {
        return Decode(data_vec, request, data_size);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(e.what());
    }
}

/**
     * @brief 编码响应对象为协议格式
     * @param response 待编码的响应对象
     * @param data_vec 输出的字节向量
     * @return 成功返回true，失败返回错误信息
     */
void DataProc::EncodeResponse(TykeResponse &response, std::vector<unsigned char> &data_vec)
{
    try
    {
        Encode(response, data_vec);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(e.what());
    }
}

/**
     * @brief 解码协议格式为响应对象
     * @param data_vec 待解码的字节向量
     * @param response 输出的响应对象
     * @param data_size 解码的数据大小
     * @return 成功返回true，失败返回错误信息
     */
nonstd::optional<bool> DataProc::DecodeResponse(const std::vector<unsigned char> &data_vec, TykeResponse &response,
                                    uint32_t &data_size)
{
    try
    {
        return Decode(data_vec, response, data_size);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(e.what());
    }
}
}// namespace tyke
