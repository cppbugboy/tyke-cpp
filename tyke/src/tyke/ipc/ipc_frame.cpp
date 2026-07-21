/**
 * @file ipc_frame.cpp
 * @brief IPC 帧解析与分片重组实现。
 *
 * 帧格式：
 * - 4 字节小端总长度（含类型字节）
 * - 1 字节类型（kMsgData 或 kMsgDataFragment）
 * - [total_len - 1] 字节载荷
 *
 * 分片载荷格式：
 * - 4 字节小端原始消息总大小
 * - 4 字节小端当前分片偏移
 * - [剩余] 分片数据块
 *
 * @author Nick
 * @date 2026/04/19
 */

#include "tyke/ipc/ipc_frame.h"

#include <string>

#include "tyke/common/log_def.h"

namespace tyke::frame
{
    /** @brief 以小端序编码 32 位无符号整数到输出向量。 */
    static void EncodeLe32Impl(const uint32_t val, std::vector<uint8_t>& out)
    {
        out.push_back(static_cast<uint8_t>(val & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    }

    /** @brief 从小端序字节数组解码 32 位无符号整数。 */
    static uint32_t DecodeLe32Impl(const uint8_t* data)
    {
        return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
            (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
    }

    /** @brief 公共接口：将 32 位值以小端序写入输出向量。 */
    void FrameParser::EncodeLe32(const uint32_t val, std::vector<uint8_t>& out)
    {
        EncodeLe32Impl(val, out);
    }

    /** @brief 公共接口：从小端序字节数组读取 32 位值。 */
    uint32_t FrameParser::DecodeLe32(const uint8_t* data)
    {
        return DecodeLe32Impl(data);
    }

    /**
     * @brief 构建 IPC 帧。
     *
     * 格式：[4B total_len (LE)] [1B type] [payload...]
     *
     * @param type 帧类型（kMsgData 或 kMsgDataFragment）
     * @param payload 载荷数据
     * @return 完整的帧数据向量
     */
    std::vector<uint8_t> FrameParser::BuildFrame(const uint8_t type, const std::vector<uint8_t>& payload)
    {
        std::vector<uint8_t> frame;
        const uint32_t total_len = 1 + static_cast<uint32_t>(payload.size());
        EncodeLe32Impl(total_len, frame);
        frame.push_back(type);
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    /**
     * @brief 从缓冲区提取一个帧。
     *
     * 解析长度前缀，验证 total_len 范围，提取类型和载荷。
     * 若帧头损坏（total_len 超出范围），丢弃 4 字节帧头并返回错误，
     * 调用方可从剩余数据重试。
     *
     * @param buffer [in,out] 输入缓冲区，成功时消费已提取的数据
     * @param type [out] 帧类型
     * @param payload [out] 载荷数据
     * @return 成功返回 true；帧头损坏或数据不完整返回错误。
     *
     * @note 损坏帧头仅丢弃 4 字节（帧头大小），保留后续有效帧数据。
     * @note 与 Go 实现一致：不丢弃整个缓冲区，仅跳过已确认无效的字节。
     */
    BoolResult FrameParser::ExtractFrame(std::vector<uint8_t>& buffer, uint8_t& type, std::vector<uint8_t>& payload)
    {
        if (buffer.size() < 5)
            return nonstd::make_unexpected("buffer too small for frame header");

        const uint32_t total_len = DecodeLe32Impl(buffer.data());
        if (total_len < 1 || total_len > kMaxFramePayloadLen + 1)
        {
            LOG_ERROR("Invalid frame header: total_len={}, discarding 4-byte header (buffer size={})",
                      total_len, buffer.size());
            const size_t discard = std::min(size_t(4), buffer.size());
            buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(discard));
            return nonstd::make_unexpected("invalid frame header: total_len out of range");
        }
        if (buffer.size() < 4 + total_len)
            return nonstd::make_unexpected(
                "buffer incomplete: expected " + std::to_string(4 + total_len) + " bytes, got " +
                std::to_string(buffer.size()));

        type = buffer[4];
        payload.assign(buffer.begin() + 5, buffer.begin() + 4 + total_len);
        buffer.erase(buffer.begin(), buffer.begin() + 4 + total_len);
        return true;
    }
} // namespace tyke::frame
