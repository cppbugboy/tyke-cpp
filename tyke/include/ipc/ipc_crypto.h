/**
 * @file ipc_crypto.h
 * @brief IPC加密模块声明。提供ECDH密钥交换和AES-GCM加解密功能。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 本模块提供IPC通信的安全加密功能，主要特性包括：
 * - ECDH P-256密钥交换：用于安全协商会话密钥
 * - HKDF-SHA256密钥派生：从共享密钥派生AES-256密钥
 * - AES-256-GCM加密：提供认证加密，防止篡改
 * - 帧协议：定义消息帧格式，支持分片传输
 *
 * @note 加密参数：
 * - 密钥交换：ECDH P-256 (OpenSSL EVP_PKEY)
 * - 密钥派生：HKDF-SHA256 (salt="tyke-v1-hkdf-salt", info="tyke-v1-aes256-key")
 * - 加密算法：AES-256-GCM (IV=12B, Tag=16B)
 * - 公钥格式：X.509 SPKI DER
 *
 * @example
 * @code
 * // ECDH密钥交换示例
 * tyke::crypto::EcdhKeyExchange alice, bob;
 * alice.GenerateKey();
 * bob.GenerateKey();
 *
 * auto alice_pub = alice.GetPublicKeyDer();
 * auto bob_pub = bob.GetPublicKeyDer();
 *
 * auto alice_secret = alice.ComputeSharedSecret(bob_pub.Value);
 * auto bob_secret = bob.ComputeSharedSecret(alice_pub.Value);
 * // alice_secret == bob_secret
 *
 * // AES-GCM加密示例
 * tyke::crypto::AesGcmCipher cipher;
 * cipher.Init(shared_secret);
 * auto encrypted = cipher.Encrypt(plaintext);
 * auto decrypted = cipher.Decrypt(encrypted.Value);
 * @endcode
 */


#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "common/tyke_def.h"

namespace tyke::crypto
{
    /// @brief 握手初始化帧类型
    constexpr uint8_t kMsgHandshakeInit = 0x01;

    /// @brief 握手响应帧类型
    constexpr uint8_t kMsgHandshakeResp = 0x02;

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


    /**
     * @class FrameParser
     * @brief 帧解析器，提供帧构建和解析功能。
     *
     * 帧格式：[4B total_len (LE)][1B frame_type][payload]
     * - total_len: 整个帧的长度（包括4B长度字段本身）
     * - frame_type: 帧类型（kMsgHandshakeInit/kMsgHandshakeResp/kMsgData/kMsgDataFragment）
     * - payload: 帧载荷
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


    /**
     * @class EcdhKeyExchange
     * @brief ECDH密钥交换类，用于安全协商共享密钥。
     *
     * 使用P-256曲线进行密钥交换，公钥以X.509 SPKI DER格式编码。
     * 内部使用OpenSSL EVP_PKEY实现。
     */
    class EcdhKeyExchange
    {
    public:
        /**
         * @brief 构造函数，初始化空实例。
         */
        EcdhKeyExchange();

        /**
         * @brief 析构函数，安全清除密钥材料。
         */
        ~EcdhKeyExchange();

        /**
         * @brief 生成ECDH密钥对。
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         */
        [[nodiscard]] BoolResult GenerateKey();

        /**
         * @brief 获取公钥的DER编码。
         *
         * @return ByteVecResult 成功返回DER编码的公钥，失败返回错误信息
         *
         * @note 返回的是X.509 SubjectPublicKeyInfo格式。
         */
        [[nodiscard]] ByteVecResult GetPublicKeyDer() const;

        /**
         * @brief 计算与对端公钥的共享密钥。
         *
         * @param peer_pub_der 对端公钥的DER编码
         *
         * @return ByteVecResult 成功返回共享密钥（32字节），失败返回错误信息
         *
         * @note 共享密钥需要通过HKDF派生后才能用于AES加密。
         */
        [[nodiscard]] ByteVecResult ComputeSharedSecret(const std::vector<uint8_t>& peer_pub_der) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };


    /**
     * @class AesGcmCipher
     * @brief AES-GCM加密类，提供认证加密功能。
     *
     * 使用AES-256-GCM算法，IV格式为[4B随机前缀][8B大端计数器]。
     * 每次加密会自动递增计数器，确保IV唯一性。
     */
    class AesGcmCipher
    {
    public:
        /**
         * @brief 构造函数，初始化未加密状态。
         */
        AesGcmCipher();

        /**
         * @brief 析构函数，安全清除密钥材料。
         */
        ~AesGcmCipher();

        /**
         * @brief 使用共享密钥初始化加密器。
         *
         * @param shared_secret ECDH共享密钥（32字节）
         *
         * @return BoolResult 成功返回true，失败返回错误信息
         *
         * @note 内部会使用HKDF-SHA256从共享密钥派生AES密钥。
         */
        [[nodiscard]] BoolResult Init(const std::vector<uint8_t>& shared_secret) const;

        /**
         * @brief 检查加密器是否已初始化。
         *
         * @return true 已初始化
         * @return false 未初始化
         */
        [[nodiscard]] bool IsInitialized() const;

        /**
         * @brief 加密数据。
         *
         * @param plaintext 明文数据
         *
         * @return ByteVecResult 成功返回密文（IV+Ciphertext+Tag），失败返回错误信息
         *
         * @note 密文格式：[IV 12B][Ciphertext NB][Auth Tag 16B]
         */
        [[nodiscard]] ByteVecResult Encrypt(const std::vector<uint8_t>& plaintext) const;

        /**
         * @brief 解密数据。
         *
         * @param ciphertext 密文数据（IV+Ciphertext+Tag）
         *
         * @return ByteVecResult 成功返回明文，失败返回错误信息
         *
         * @note 如果数据被篡改或密钥不正确，会返回认证失败错误。
         */
        [[nodiscard]] ByteVecResult Decrypt(const std::vector<uint8_t>& ciphertext) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace tyke::crypto