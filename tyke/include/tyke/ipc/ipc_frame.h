/**
 * @file ipc_frame.h
 * @brief IPC 帧解析与分片重组声明。提供消息帧的构建、解析及大消息分片重组功能。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 本模块提供 IPC 传输层的分帧能力（与加密无关），主要特性包括：
 * - 帧格式：[4B total_len (LE)][1B frame_type][payload]
 * - 大消息分片：>64KB 消息按 64KB 切片，每片带 [total_size][offset] 头用于重组
 * - 最大帧载荷：16MB
 *
 * @note 自 2026-06 起，IPC 传输不再使用加密，所有帧载荷为明文。
 */


#pragma once

#include <cstdint>
#include <vector>

#include "tyke/common/tyke_def.h"

namespace tyke::frame
{
    /// @brief 数据帧类型（单帧消息）
    constexpr uint8_t kMsgData = 0x03;

    /// @brief 分片数据帧类型（大消息分片）
    constexpr uint8_t kMsgDataFragment = 0x04;

    /// @brief 最大帧载荷长度（16MB）
    constexpr uint32_t kMaxFramePayloadLen = 16 * 1024 * 1024;

    /// @brief 分片块大小（64KB）
    constexpr uint32_t kFragmentChunkSize = 64 * 1024;

    /// @brief 分片头大小（8B: total_size[4] + offset[4]）
    constexpr uint32_t kFragmentHeaderSize = 8;

    /// @brief 分片重组后的逻辑消息最大大小（64MB），防止恶意 totalSize 触发 OOM
    constexpr uint32_t kMaxMessageSize = 64 * 1024 * 1024;


    /**
     * @class FrameParser
     * @brief 帧解析器，提供帧构建和解析功能。
     *
     * 帧格式：[4B total_len (LE)][1B frame_type][payload]
     * - total_len: 载荷长度（1B frame_type + payload 字节数，不含 4B 长度字段本身）
     * - frame_type: 帧类型（kMsgData / kMsgDataFragment）
     * - payload: 帧载荷（明文）
     */
    class FrameParser
    {
    public:
        /**
         * @brief 构建帧。
         *
         * @param type 帧类型
         * @param payload 载荷数据
         *
         * @return std::vector<uint8_t> 完整的帧数据
         */
        static std::vector<uint8_t> BuildFrame(uint8_t type, const std::vector<uint8_t>& payload);

        /**
         * @brief 从缓冲区提取帧。
         *
         * @param buffer 输入/输出缓冲区，成功提取后会移除已提取的数据
         * @param type 输出：帧类型
         * @param payload 输出：载荷数据
         *
         * @return BoolResult 成功返回true，数据不完整返回错误
         */
        static BoolResult ExtractFrame(std::vector<uint8_t>& buffer, uint8_t& type, std::vector<uint8_t>& payload);

        /**
         * @brief 编码32位小端整数。
         *
         * @param val 要编码的值
         * @param out 输出缓冲区
         */
        static void EncodeLe32(uint32_t val, std::vector<uint8_t>& out);

        /**
         * @brief 解码32位小端整数。
         *
         * @param data 数据指针（至少4字节）
         *
         * @return uint32_t 解码后的值
         */
        static uint32_t DecodeLe32(const uint8_t* data);
    };
} // namespace tyke::frame
