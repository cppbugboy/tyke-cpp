/**
 * @file data_proc.cpp
 * @brief 数据编解码实现，包含协议序列化和反序列化。
 * @author Nick
 * @date 2026/04/20
 */

#include "tyke/core/data_proc.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "tyke/common/log_def.h"

namespace tyke
{
    inline void write_le32(unsigned char* buf, const uint32_t val)
    {
        buf[0] = static_cast<unsigned char>(val & 0xFF);
        buf[1] = static_cast<unsigned char>((val >> 8) & 0xFF);
        buf[2] = static_cast<unsigned char>((val >> 16) & 0xFF);
        buf[3] = static_cast<unsigned char>((val >> 24) & 0xFF);
    }

    inline uint32_t read_le32(const unsigned char* buf)
    {
        return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
            (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
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
        hdr.msg_type = static_cast<MessageType>(read_le32(data + 4));
        hdr.reserved[0] = read_le32(data + 8);
        hdr.reserved[1] = read_le32(data + 12);
        hdr.reserved[2] = read_le32(data + 16);
        hdr.metadata_len = read_le32(data + 20);
        hdr.content_len = read_le32(data + 24);
    }

    void DataProc::EncodeRequest(Request& request, std::vector<uint8_t>& data_vec)
    {
        LOG_DEBUG("Encoding request, route={}", request.GetRoute());
        Encode(request, data_vec);
    }

    std::optional<bool> DataProc::DecodeRequest(const std::vector<uint8_t>& data_vec, Request& request,
                                                uint32_t& data_size)
    {
        LOG_DEBUG("Decoding request, size={}", data_vec.size());
        return Decode(data_vec, request, data_size);
    }

    void DataProc::EncodeResponse(Response& response, std::vector<uint8_t>& data_vec)
    {
        LOG_DEBUG("Encoding response, route={}", response.GetRoute());
        Encode(response, data_vec);
    }

    std::optional<bool> DataProc::DecodeResponse(const std::vector<uint8_t>& data_vec, Response& response,
                                                 uint32_t& data_size)
    {
        LOG_DEBUG("Decoding response, size={}", data_vec.size());
        return Decode(data_vec, response, data_size);
    }

    bool DataProc::PeekHeader(const unsigned char* data, const size_t size, ProtocolHeader& header)
    {
        if (constexpr size_t header_size = sizeof(ProtocolHeader); size < header_size)
        {
            return false;
        }
        deserialize_header(data, header);
        return true;
    }
} // namespace tyke