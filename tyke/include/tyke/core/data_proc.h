/**
 * @file data_proc.h
 * @brief 数据编解码声明。提供请求/响应对象与协议字节流之间的序列化/反序列化功能。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <climits>

#include "tyke/common/log_def.h"
#include "request.h"
#include "response_metadata.h"

namespace tyke
{
    /** @brief 元数据最大长度: 4MB。 */
    constexpr uint32_t kMaxMetadataLen = 4 * 1024 * 1024;
    /** @brief 内容最大长度: 64MB。 */
    constexpr uint32_t kMaxContentLen = 64 * 1024 * 1024;

    /** @brief 数据编解码工具类。提供请求/响应对象与协议字节流之间的序列化/反序列化。 */
    class DataProc
    {
    public:
        DataProc() = delete;
        ~DataProc() = delete;

        /** @brief 将请求对象编码为协议字节流。 */
        static void EncodeRequest(Request& request, std::vector<uint8_t>& data_vec);

        /** @brief 从协议字节流解码为请求对象。
         * @param data_vec 输入的字节流。
         * @param request 输出解码后的请求。
         * @param data_size 输出实际消耗的字节数。
         * @return 解码成功返回 true，失败返回 nullopt。
         */
        static std::optional<bool> DecodeRequest(const std::vector<uint8_t>& data_vec, Request& request,
                                                 uint32_t& data_size);

        /** @brief 将响应对象编码为协议字节流。 */
        static void EncodeResponse(Response& response, std::vector<uint8_t>& data_vec);

        /** @brief 从协议字节流解码为响应对象。
         * @param data_vec 输入的字节流。
         * @param response 输出解码后的响应。
         * @param data_size 输出实际消耗的字节数。
         * @return 解码成功返回 true，失败返回 nullopt。
         */
        static std::optional<bool> DecodeResponse(const std::vector<uint8_t>& data_vec, Response& response,
                                                  uint32_t& data_size);

        /** @brief 从字节流中提取协议头，不消费数据。
         * @param data 数据指针。
         * @param size 数据长度。
         * @param header 输出解析后的协议头。
         * @return 解析成功返回 true。
         */
        static bool PeekHeader(const unsigned char* data, size_t size, ProtocolHeader& header);

        /** @brief 模板编码方法。将任意带 metadata_/content_ 的消息对象序列化为字节流。
         * @tparam T 消息类型（Request 或 Response）。
         * @param msg 待编码的消息对象。
         * @param data_vec 输出字节流。
         * @throw std::runtime_error 序列化失败或数据超过协议限制时抛出。
         */
        template <typename T>
        static void Encode(T& msg, std::vector<uint8_t>& data_vec)
        {
            try
            {
                std::string metadata_string;
                auto result = msg.metadata_.ToJsonString(metadata_string);
                if (!result)
                {
                    LOG_ERROR("Failed to serialize metadata: {}", result.error());
                    throw std::runtime_error("Failed to serialize metadata: " + result.error());
                }

                constexpr size_t header_size = sizeof(ProtocolHeader);
                const size_t meta_size = metadata_string.size();
                const size_t content_size = msg.content_.size();

                // 防御性检查：确保长度可以安全存入 uint32_t
                if (meta_size > static_cast<size_t>(UINT32_MAX) || content_size > static_cast<size_t>(UINT32_MAX))
                {
                    LOG_ERROR("Metadata or content too large for uint32_t: meta={}, content={}", meta_size, content_size);
                    throw std::runtime_error("Data too large for protocol");
                }

                const size_t total_size = header_size + meta_size + content_size;

                if (data_vec.capacity() < total_size)
                {
                    data_vec.reserve(total_size);
                }
                data_vec.resize(total_size);

                msg.protocol_header_.metadata_len = static_cast<uint32_t>(meta_size);
                msg.protocol_header_.content_len = static_cast<uint32_t>(content_size);

                unsigned char* ptr = data_vec.data();

                serialize_header(msg.protocol_header_, ptr);
                ptr += header_size;

                if (meta_size > 0)
                {
                    std::memcpy(ptr, metadata_string.data(), meta_size);
                    ptr += meta_size;
                }

                if (content_size > 0)
                {
                    std::memcpy(ptr, msg.content_.data(), content_size);
                }

                LOG_DEBUG("Encode completed: header={} bytes, metadata={} bytes, content={} bytes, total={} bytes",
                          header_size, meta_size, content_size, total_size);
            }
            catch (const nlohmann::json::exception& e)
            {
                LOG_ERROR("JSON encode error: id={}, message={}", e.id, e.what());
                throw;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Encode exception: {}", e.what());
                throw;
            }
        }

        /** @brief 模板解码方法。从字节流中还原消息对象。
         * @tparam T 消息类型（Request 或 Response）。
         * @param data_vec 输入的字节流。
         * @param msg 输出解码后的消息对象。
         * @param data_size 输出实际消耗的字节数。
         * @return 解码成功返回 true，失败返回 nullopt。
         */
        template <typename T>
        static std::optional<bool> Decode(const std::vector<uint8_t>& data_vec, T& msg, uint32_t& data_size)
        {
            try
            {
                data_size = 0;
                const size_t vec_size = data_vec.size();
                constexpr size_t header_size = sizeof(ProtocolHeader);

                if (vec_size < header_size)
                {
                    LOG_ERROR("Data too short for header: expected {} bytes, got {}", header_size, vec_size);
                    return false;
                }

                deserialize_header(data_vec.data(), msg.protocol_header_);

                if (std::memcmp(msg.protocol_header_.magic, kProtocolMagic, sizeof(msg.protocol_header_.magic)) != 0)
                {
                    LOG_ERROR("Protocol magic mismatch: expected TYKE");
                    return false;
                }

                const uint32_t meta_len = msg.protocol_header_.metadata_len;
                const uint32_t cont_len = msg.protocol_header_.content_len;

                if (meta_len > kMaxMetadataLen)
                {
                    LOG_ERROR("Metadata length exceeds limit: {} > {}", meta_len, kMaxMetadataLen);
                    return false;
                }

                if (cont_len > kMaxContentLen)
                {
                    LOG_ERROR("Content length exceeds limit: {} > {}", cont_len, kMaxContentLen);
                    return false;
                }

                // 防御性检查：防止 meta_len + cont_len 整数溢出
                if (static_cast<uint64_t>(meta_len) + static_cast<uint64_t>(cont_len) > static_cast<uint64_t>(UINT32_MAX))
                {
                    LOG_ERROR("Metadata + content length overflow: {} + {}", meta_len, cont_len);
                    return false;
                }

                if (vec_size < header_size + meta_len + cont_len)
                {
                    LOG_ERROR("Data incomplete: expected {} bytes, got {}", header_size + meta_len + cont_len,
                              vec_size);
                    return false;
                }

                msg.metadata_.Clear();

                if (meta_len > 0)
                {
                    std::string meta_str(reinterpret_cast<const char*>(data_vec.data() + header_size), meta_len);
                    auto result = msg.metadata_.FromJsonString(meta_str);
                    if (!result)
                    {
                        LOG_ERROR("Failed to parse metadata: {}", result.error());
                        return false;
                    }
                }

                const unsigned char* content_start = data_vec.data() + header_size + meta_len;
                msg.content_.assign(content_start, content_start + cont_len);

                data_size = static_cast<uint32_t>(header_size + meta_len + cont_len);
                LOG_DEBUG("Decode completed: header={} bytes, metadata={} bytes, content={} bytes, total={} bytes",
                          header_size, meta_len, cont_len, data_size);
            }
            catch (const nlohmann::json::exception& e)
            {
                LOG_ERROR("JSON decode error: id={}, message={}", e.id, e.what());
                return false;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Decode exception: {}", e.what());
                return false;
            }
            catch (...)
            {
                LOG_ERROR("Unknown decode exception");
                return false;
            }

            return true;
        }
    };
}; // namespace tyke
