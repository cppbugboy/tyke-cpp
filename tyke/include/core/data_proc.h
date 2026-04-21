/**
 * @file data_proc.h
 * @brief 数据编解码声明。提供请求/响应对象与协议字节流之间的序列化/反序列化功能。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include "tyke_request.h"
#include "response_metadata.h"
#include "common/tyke_result.h"

namespace tyke
{
    
    class DataProc
    {
    public:
        
        static void EncodeRequest(TykeRequest& request, std::vector<unsigned char>& data_vec);

        
        static std::optional<bool> DecodeRequest(const std::vector<unsigned char>& data_vec, TykeRequest& request,
                                  uint32_t& data_size);

        
        static void EncodeResponse(TykeResponse& response, std::vector<unsigned char>& data_vec);

        
        static std::optional<bool> DecodeResponse(const std::vector<unsigned char>& data_vec, TykeResponse& response,
                                   uint32_t& data_size);

        static bool PeekHeader(const unsigned char* data, size_t size, ProtocolHeader& header);

    private:
        
        template <typename T>
        static void Encode(T& msg, std::vector<unsigned char>& data_vec);

        
        template <typename T>
        static std::optional<bool> Decode(const std::vector<unsigned char>& data_vec, T& msg, uint32_t& data_size);
    };
}
