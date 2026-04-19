#include "core/data_proc.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "common/log_def.h"

namespace tyke
{
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

        std::memcpy(ptr, &msg.protocol_header_, header_size);
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

        std::memcpy(&msg.protocol_header_, data_vec.data(), header_size);

        if (std::memcmp(msg.protocol_header_.magic, kProtocolMagic, sizeof(msg.protocol_header_.magic)) != 0)
        {
            LOG_ERROR("Protocol magic mismatch: expected TYKE");
            return false;
        }

        const uint32_t meta_len = msg.protocol_header_.metadata_len;
        const uint32_t cont_len = msg.protocol_header_.content_len;

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
    Encode(request, data_vec);
}
std::optional<bool> DataProc::DecodeRequest(const std::vector<unsigned char> &data_vec, TykeRequest &request,
                                   uint32_t &data_size)
{
    return Decode(data_vec, request, data_size);
}
void DataProc::EncodeResponse(TykeResponse &response, std::vector<unsigned char> &data_vec)
{
    Encode(response, data_vec);
}
std::optional<bool> DataProc::DecodeResponse(const std::vector<unsigned char> &data_vec, TykeResponse &response,
                                    uint32_t &data_size)
{
    return Decode(data_vec, response, data_size);
}
}// namespace tyke

