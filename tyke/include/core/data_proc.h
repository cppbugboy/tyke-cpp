/**
 * @file data_proc.h
 * @brief 数据编解码处理器
 * @author Nick
 * @date 2026/04/17
 *
 * 提供请求和响应的序列化与反序列化功能，实现Tyke协议格式与对象之间的转换。
 * 协议格式: [ProtocolHeader][Metadata JSON][Content Binary]
 */

#ifndef TYKE_DATA_PROC_H
#define TYKE_DATA_PROC_H
#include "tyke_request.h"
#include "response_metadata.h"
#include "common/tyke_def.h"
#include "common/tyke_result.h"

namespace tyke
{
    /**
     * @brief 数据编解码处理器类
     *
     * 提供静态方法实现请求和响应的编解码功能，支持Tyke协议格式。
     * 协议格式: [ProtocolHeader(固定28字节)][Metadata JSON][Content Binary]
     *
     * 使用示例：
     * @code
     *   std::vector<unsigned char> data;
     *   auto result = DataProc::EncodeRequest(request, data);
     *   if (result) {
     *       // 发送data
     *   }
     * @endcode
     */
    class DataProc
    {
    public:
        /**
         * @brief 编码请求对象为协议格式
         * @param request 待编码的请求对象
         * @param data_vec 输出的字节向量
         * @return 成功返回true，失败返回错误信息
         *
         * 将TykeRequest对象编码为Tyke协议格式的字节流，包括协议头、元数据JSON和内容数据。
         */
        static void EncodeRequest(TykeRequest& request, std::vector<unsigned char>& data_vec);

        /**
         * @brief 解码协议格式为请求对象
         * @param data_vec 待解码的字节向量
         * @param request 输出的请求对象
         * @param data_size 解码的数据大小
         * @return 成功返回true，失败返回错误信息
         *
         * 从Tyke协议格式的字节流中解析出TykeRequest对象。
         */
        static nonstd::optional<bool> DecodeRequest(const std::vector<unsigned char>& data_vec, TykeRequest& request,
                                  uint32_t& data_size);

        /**
         * @brief 编码响应对象为协议格式
         * @param response 待编码的响应对象
         * @param data_vec 输出的字节向量
         * @return 成功返回true，失败返回错误信息
         *
         * 将TykeResponse对象编码为Tyke协议格式的字节流。
         */
        static void EncodeResponse(TykeResponse& response, std::vector<unsigned char>& data_vec);

        /**
         * @brief 解码协议格式为响应对象
         * @param data_vec 待解码的字节向量
         * @param response 输出的响应对象
         * @param data_size 解码的数据大小
         * @return 成功返回true，失败返回错误信息
         *
         * 从Tyke协议格式的字节流中解析出TykeResponse对象。
         */
        static nonstd::optional<bool> DecodeResponse(const std::vector<unsigned char>& data_vec, TykeResponse& response,
                                   uint32_t& data_size);

    private:
        /**
         * @brief 通用编码模板函数
         * @tparam T 请求或响应类型
         * @param msg 待编码的消息对象
         * @param data_vec 输出的字节向量
         * @return 成功返回true，失败返回错误信息
         *
         * 实现请求和响应的通用编码逻辑，包括元数据JSON序列化和协议头组装。
         */
        template <typename T>
        static void Encode(T& msg, std::vector<unsigned char>& data_vec);

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
        template <typename T>
        static nonstd::optional<bool> Decode(const std::vector<unsigned char>& data_vec, T& msg, uint32_t& data_size);
    };
} // tyke

#endif //TYKE_DATA_PROC_H