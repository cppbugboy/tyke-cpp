/**
 * @file data_proc.cpp
 * @brief 数据编解码实现，包含协议序列化和反序列化。
 * @author Nick
 * @date 2026/04/20
 */

#include "core/data_proc.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "common/log_def.h"

namespace tyke
{
namespace
{
constexpr uint32_t kMaxMetadataLen = 4 * 1024 * 1024;
constexpr uint32_t kMaxContentLen  = 64 * 1024 * 1024;

inline void write_le32(unsigned char* buf, uint32_t val)
{
    buf[0] = static_cast<unsigned char>(val & 0xFF);
    buf[1] = static_cast<unsigned char>((val >> 8) & 0xFF);
    buf[2] = static_cast<unsigned char>((val >> 16) & 0xFF);
    buf[3] = static_cast<unsigned char>((val >> 24) & 0xFF);
}

inline uint32_t read_le32(const unsigned char* buf)
{
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

void serialize_header(const ProtocolHeader& hdr, unsigned char* out)
{
    std::memcpy(out, hdr.magic, 4);
    write_le32(out + 4, static_cast<uint32_t>(hdr.msg_type));
    for (int i = 0; i < 3; ++i)
        write_le32(out + 8 + i * 4, hdr.reserved[i]);
    write_le32(out + 20, hdr.metadata_len);
    write_le32(out + 24, hdr.content_len);
}

void deserialize_header(const unsigned char* data, ProtocolHeader& hdr)
{
    std::memcpy(hdr.magic, data, 4);
    hdr.msg_type      = static_cast<MessageType>(read_le32(data + 4));
    hdr.reserved[0]   = read_le32(data + 8);
    hdr.reserved[1]   = read_le32(data + 12);
    hdr.reserved[2]   = read_le32(data + 16);
    hdr.metadata_len  = read_le32(data + 20);
    hdr.content_len   = read_le32(data + 24);
}
}
template<typename T>
void DataProc::Encode(T &msg, std::vector<unsigned char> &data_vec)
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

        constexpr size_t header_size  = sizeof(ProtocolHeader);
        const size_t meta_size    = metadata_string.size();
        const size_t content_size = msg.content_.size();
        const size_t total_size   = header_size + meta_size + content_size;

        data_vec.clear();
        data_vec.resize(total_size);

        msg.protocol_header_.metadata_len = static_cast<uint32_t>(meta_size);
        msg.protocol_header_.content_len  = static_cast<uint32_t>(content_size);

        unsigned char *ptr = data_vec.data();

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

        LOG_DEBUG("Encode completed: header={} bytes, metadata={} bytes, content={} bytes, total={} bytes", header_size,
                  meta_size, content_size, total_size);
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
template<typename T>
std::optional<bool> DataProc::Decode(const std::vector<unsigned char> &data_vec, T &msg, uint32_t &data_size)
{
    try
    {
        data_size                = 0;
        const size_t vec_size    = data_vec.size();
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

        if (vec_size < header_size + meta_len + cont_len)
        {
            LOG_ERROR("Data incomplete: expected {} bytes, got {}", header_size + meta_len + cont_len, vec_size);
            return false;
        }

        if (meta_len > 0)
        {
            std::string meta_str(reinterpret_cast<const char *>(data_vec.data() + header_size), meta_len);
            auto result = msg.metadata_.FromJsonString(meta_str);
            if (!result)
            {
                LOG_ERROR("Failed to parse metadata: {}", result.error());
                return false;
            }
        }

        const unsigned char *content_start = data_vec.data() + header_size + meta_len;
        msg.content_.assign(content_start, content_start + cont_len);

        data_size = static_cast<uint32_t>(header_size + meta_len + cont_len);
        LOG_DEBUG("Decode completed: header={} bytes, metadata={} bytes, content={} bytes, total={} bytes", header_size,
                  meta_len, cont_len, data_size);
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
void DataProc::EncodeRequest(TykeRequest &request, std::vector<unsigned char> &data_vec)
{
    LOG_INFO("Encoding request, route={}", request.GetRoute());
    Encode(request, data_vec);
}
std::optional<bool> DataProc::DecodeRequest(const std::vector<unsigned char> &data_vec, TykeRequest &request,
                                   uint32_t &data_size)
{
    LOG_INFO("Decoding request, size={}", data_vec.size());
    return Decode(data_vec, request, data_size);
}
void DataProc::EncodeResponse(TykeResponse &response, std::vector<unsigned char> &data_vec)
{
    LOG_INFO("Encoding response, route={}", response.GetRoute());
    Encode(response, data_vec);
}
std::optional<bool> DataProc::DecodeResponse(const std::vector<unsigned char> &data_vec, TykeResponse &response,
                                    uint32_t &data_size)
{
    LOG_INFO("Decoding response, size={}", data_vec.size());
    return Decode(data_vec, response, data_size);
}
}// namespace tyke

