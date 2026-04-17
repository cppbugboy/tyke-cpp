/**
 * @file ipc_crypto.h
 * @brief IPC加密模块
 * @author Nick
 * @date 2026/04/17
 *
 * 提供IPC通信的加密功能，包括帧解析、ECDH密钥交换和AES-GCM加密。
 */

#ifndef IPC_CRYPTO_H_
#define IPC_CRYPTO_H_

#include <cstdint>
#include <vector>
#include <memory>
#include "common/tyke_result.h"

namespace tyke
{
    /**
     * @brief 加密模块命名空间
     *
     * 包含帧解析、密钥交换和加密算法的实现。
     */
    namespace crypto
    {
        /// 握手初始化消息类型
        constexpr uint8_t kMsgHandshakeInit = 0x01;

        /// 握手响应消息类型
        constexpr uint8_t kMsgHandshakeResp = 0x02;

        /// 数据消息类型
        constexpr uint8_t kMsgData = 0x03;

        /**
         * @brief 帧解析器类
         *
         * 提供帧的构建和解析功能，用于IPC消息的封装。
         */
        class FrameParser
        {
        public:
            /**
             * @brief 构建帧
             * @param type 消息类型
             * @param payload 负载数据
             * @return 构建好的帧数据
             */
            static std::vector<uint8_t> BuildFrame(uint8_t type, const std::vector<uint8_t>& payload);

            /**
             * @brief 提取帧
             * @param buffer 输入缓冲区（会被修改）
             * @param type 输出消息类型
             * @param payload 输出负载数据
             * @return 成功返回true，失败返回错误信息
             */
            static BoolResult ExtractFrame(std::vector<uint8_t>& buffer, uint8_t& type, std::vector<uint8_t>& payload);
        };

        /**
         * @brief ECDH密钥交换类
         *
         * 实现椭圆曲线Diffie-Hellman密钥交换，用于安全协商共享密钥。
         */
        class EcdhKeyExchange
        {
        public:
            /**
             * @brief 构造函数
             */
            EcdhKeyExchange();

            /**
             * @brief 析构函数
             */
            ~EcdhKeyExchange();

            /**
             * @brief 生成密钥对
             * @return 成功返回true，失败返回错误信息
             */
            BoolResult GenerateKey();

            /**
             * @brief 获取公钥DER编码
             * @return 公钥DER编码字节向量
             */
            ByteVecResult GetPublicKeyDer() const;

            /**
             * @brief 计算共享密钥
             * @param peer_pub_der 对方公钥DER编码
             * @return 共享密钥字节向量
             */
            ByteVecResult ComputeSharedSecret(const std::vector<uint8_t>& peer_pub_der) const;

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;  ///< 实现细节
        };

        /**
         * @brief AES-GCM加密类
         *
         * 实现AES-GCM对称加密算法，用于IPC通信数据的加密和解密。
         */
        class AesGcmCipher
        {
        public:
            /**
             * @brief 构造函数
             */
            AesGcmCipher();

            /**
             * @brief 析构函数
             */
            ~AesGcmCipher();

            /**
             * @brief 初始化加密器
             * @param shared_secret 共享密钥
             * @return 成功返回true，失败返回错误信息
             */
            BoolResult Init(const std::vector<uint8_t>& shared_secret);

            /**
             * @brief 检查是否已初始化
             * @return 已初始化返回true，否则返回false
             */
            bool IsInitialized() const;

            /**
             * @brief 加密数据
             * @param plaintext 明文数据
             * @return 密文数据
             */
            ByteVecResult Encrypt(const std::vector<uint8_t>& plaintext) const;

            /**
             * @brief 解密数据
             * @param ciphertext 密文数据
             * @return 明文数据
             */
            ByteVecResult Decrypt(const std::vector<uint8_t>& ciphertext) const;

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;  ///< 实现细节
        };
    } // namespace crypto
} // namespace tyke

#endif // IPC_CRYPTO_H_
