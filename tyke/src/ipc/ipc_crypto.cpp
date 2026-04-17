/**
 * @file ipc_crypto.cpp
 * @brief IPC加密模块实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现IPC通信的加密功能，包括帧解析、ECDH密钥交换和AES-GCM加密。
 * 使用OpenSSL库实现加密算法。
 */

#include "ipc/ipc_crypto.h"
#include "common/log_def.h"

#include <cstring>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/core_names.h>
#include <openssl/rand.h>

namespace tyke
{
    namespace crypto
    {
        /**
         * @brief 编码32位无符号整数为大端字节序
         * @param val 待编码的值
         * @param out 输出字节向量
         */
        static void EncodeU32(uint32_t val, std::vector<uint8_t>& out)
        {
            out.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
            out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>(val & 0xFF));
        }

        /**
         * @brief 从大端字节序解码32位无符号整数
         * @param data 数据指针
         * @return 解码后的值
         */
        static uint32_t DecodeU32(const uint8_t* data)
        {
            return (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8) |
                   static_cast<uint32_t>(data[3]);
        }

        /**
         * @brief 构建帧
         * @param type 消息类型
         * @param payload 负载数据
         * @return 构建好的帧数据
         *
         * 帧格式: [4字节总长度][1字节类型][负载]
         */
        std::vector<uint8_t> FrameParser::BuildFrame(uint8_t type, const std::vector<uint8_t>& payload)
        {
            std::vector<uint8_t> frame;
            uint32_t total_len = 1 + static_cast<uint32_t>(payload.size());
            EncodeU32(total_len, frame);
            frame.push_back(type);
            frame.insert(frame.end(), payload.begin(), payload.end());
            return frame;
        }

        /**
         * @brief 提取帧
         * @param buffer 输入缓冲区（会被修改）
         * @param type 输出消息类型
         * @param payload 输出负载数据
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult FrameParser::ExtractFrame(std::vector<uint8_t>& buffer, uint8_t& type, std::vector<uint8_t>& payload)
        {
            if (buffer.size() < 5)
                return nonstd::make_unexpected("buffer too small for frame header");

            uint32_t total_len = DecodeU32(buffer.data());
            if (buffer.size() < 4 + total_len)
                return nonstd::make_unexpected("buffer incomplete: expected " +
                    std::to_string(4 + total_len) + " bytes, got " + std::to_string(buffer.size()));

            type = buffer[4];
            payload.assign(buffer.begin() + 5, buffer.begin() + 4 + total_len);
            buffer.erase(buffer.begin(), buffer.begin() + 4 + total_len);
            return true;
        }

        /// EcdhKeyExchange实现结构
        struct EcdhKeyExchange::Impl
        {
            EVP_PKEY* pkey = nullptr;  ///< OpenSSL密钥对象
        };

        EcdhKeyExchange::EcdhKeyExchange() : impl_(new Impl())
        {
        }

        EcdhKeyExchange::~EcdhKeyExchange()
        {
            if (impl_->pkey)
            {
                EVP_PKEY_free(impl_->pkey);
            }
        }

        /**
         * @brief 生成密钥对
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult EcdhKeyExchange::GenerateKey()
        {
            if (impl_->pkey)
            {
                EVP_PKEY_free(impl_->pkey);
                impl_->pkey = nullptr;
            }

            impl_->pkey = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1");
            if (!impl_->pkey)
            {
                LOG_ERROR("ECDH key generation failed");
                return nonstd::make_unexpected("ECDH key generation failed");
            }
            LOG_DEBUG("ECDH key generated successfully");
            return true;
        }

        /**
         * @brief 获取公钥DER编码
         * @return 公钥DER编码字节向量
         */
        ByteVecResult EcdhKeyExchange::GetPublicKeyDer() const
        {
            if (!impl_->pkey)
            {
                return nonstd::make_unexpected("no ECDH key available");
            }

            int len = i2d_PUBKEY(impl_->pkey, nullptr);
            if (len <= 0)
            {
                LOG_ERROR("Failed to get public key DER length");
                return nonstd::make_unexpected("failed to get public key DER length");
            }

            std::vector<uint8_t> der(static_cast<size_t>(len));
            uint8_t* ptr = der.data();
            if (i2d_PUBKEY(impl_->pkey, &ptr) <= 0)
            {
                LOG_ERROR("Failed to export public key DER");
                return nonstd::make_unexpected("failed to export public key DER");
            }
            return der;
        }

        /**
         * @brief 计算共享密钥
         * @param peer_pub_der 对方公钥DER编码
         * @return 共享密钥字节向量
         */
        ByteVecResult EcdhKeyExchange::ComputeSharedSecret(const std::vector<uint8_t>& peer_pub_der) const
        {
            if (!impl_->pkey)
            {
                return nonstd::make_unexpected("no ECDH key available");
            }

            const uint8_t* ptr = peer_pub_der.data();
            EVP_PKEY* peer_pkey = d2i_PUBKEY(nullptr, &ptr, static_cast<long>(peer_pub_der.size()));
            if (!peer_pkey)
            {
                LOG_ERROR("Failed to parse peer public key DER");
                return nonstd::make_unexpected("failed to parse peer public key DER");
            }

            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(impl_->pkey, nullptr);
            if (!ctx)
            {
                EVP_PKEY_free(peer_pkey);
                LOG_ERROR("Failed to create PKEY context");
                return nonstd::make_unexpected("failed to create PKEY context");
            }

            std::vector<uint8_t> secret;
            if (EVP_PKEY_derive_init(ctx) <= 0 ||
                EVP_PKEY_derive_set_peer(ctx, peer_pkey) <= 0)
            {
                EVP_PKEY_CTX_free(ctx);
                EVP_PKEY_free(peer_pkey);
                LOG_ERROR("Failed to initialize ECDH derivation");
                return nonstd::make_unexpected("failed to initialize ECDH derivation");
            }

            size_t secret_len = 0;
            if (EVP_PKEY_derive(ctx, nullptr, &secret_len) <= 0)
            {
                EVP_PKEY_CTX_free(ctx);
                EVP_PKEY_free(peer_pkey);
                LOG_ERROR("Failed to determine shared secret length");
                return nonstd::make_unexpected("failed to determine shared secret length");
            }

            secret.resize(secret_len);
            if (EVP_PKEY_derive(ctx, secret.data(), &secret_len) <= 0)
            {
                EVP_PKEY_CTX_free(ctx);
                EVP_PKEY_free(peer_pkey);
                LOG_ERROR("Failed to compute shared secret");
                return nonstd::make_unexpected("failed to compute shared secret");
            }

            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(peer_pkey);
            LOG_DEBUG("Shared secret computed successfully, length={}", secret_len);
            return secret;
        }

        /// AesGcmCipher实现结构
        struct AesGcmCipher::Impl
        {
            std::vector<uint8_t> aes_key;   ///< AES密钥
            bool initialized = false;        ///< 是否已初始化
        };

        AesGcmCipher::AesGcmCipher() : impl_(new Impl())
        {
        }

        AesGcmCipher::~AesGcmCipher()
        {
        }

        /**
         * @brief 初始化加密器
         * @param shared_secret 共享密钥
         * @return 成功返回true，失败返回错误信息
         */
        BoolResult AesGcmCipher::Init(const std::vector<uint8_t>& shared_secret)
        {
            if (shared_secret.empty())
            {
                return nonstd::make_unexpected("shared secret is empty");
            }

            // 使用SHA-256派生AES-256密钥
            impl_->aes_key.resize(32);
            if (!EVP_Q_digest(nullptr, "SHA256", nullptr, shared_secret.data(), shared_secret.size(),
                              impl_->aes_key.data(), nullptr))
            {
                LOG_ERROR("SHA256 digest failed");
                return nonstd::make_unexpected("SHA256 digest failed");
            }

            impl_->initialized = true;
            LOG_DEBUG("AES-GCM cipher initialized");
            return true;
        }

        bool AesGcmCipher::IsInitialized() const
        {
            return impl_->initialized;
        }

        /**
         * @brief 加密数据
         * @param plaintext 明文数据
         * @return 密文数据（格式: [12字节IV][密文][16字节Tag]）
         */
        ByteVecResult AesGcmCipher::Encrypt(const std::vector<uint8_t>& plaintext) const
        {
            if (!impl_->initialized)
            {
                return nonstd::make_unexpected("cipher not initialized");
            }

            // 生成随机IV
            std::vector<uint8_t> iv(12);
            if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1)
            {
                LOG_ERROR("RAND_bytes for IV failed");
                return nonstd::make_unexpected("RAND_bytes for IV failed");
            }

            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            if (!ctx)
            {
                LOG_ERROR("Failed to create cipher context");
                return nonstd::make_unexpected("failed to create cipher context");
            }

            if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
                EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
                EVP_EncryptInit_ex(ctx, nullptr, nullptr, impl_->aes_key.data(), iv.data()) != 1)
            {
                EVP_CIPHER_CTX_free(ctx);
                LOG_ERROR("AES-GCM encrypt init failed");
                return nonstd::make_unexpected("AES-GCM encrypt init failed");
            }

            std::vector<uint8_t> ciphertext(plaintext.size());
            int out_len = 0;
            if (EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len, plaintext.data(),
                                  static_cast<int>(plaintext.size())) != 1)
            {
                EVP_CIPHER_CTX_free(ctx);
                LOG_ERROR("AES-GCM encrypt update failed");
                return nonstd::make_unexpected("AES-GCM encrypt update failed");
            }

            int final_len = 0;
            if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len, &final_len) != 1)
            {
                EVP_CIPHER_CTX_free(ctx);
                LOG_ERROR("AES-GCM encrypt final failed");
                return nonstd::make_unexpected("AES-GCM encrypt final failed");
            }

            // 获取认证标签
            std::vector<uint8_t> tag(16);
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1)
            {
                EVP_CIPHER_CTX_free(ctx);
                LOG_ERROR("AES-GCM get tag failed");
                return nonstd::make_unexpected("AES-GCM get tag failed");
            }

            EVP_CIPHER_CTX_free(ctx);

            // 组装结果: IV + 密文 + Tag
            std::vector<uint8_t> result;
            result.reserve(12 + ciphertext.size() + 16);
            result.insert(result.end(), iv.begin(), iv.end());
            result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + out_len + final_len);
            result.insert(result.end(), tag.begin(), tag.end());
            return result;
        }

        /**
         * @brief 解密数据
         * @param ciphertext 密文数据（格式: [12字节IV][密文][16字节Tag]）
         * @return 明文数据
         */
        ByteVecResult AesGcmCipher::Decrypt(const std::vector<uint8_t>& ciphertext) const
        {
            if (!impl_->initialized)
            {
                return nonstd::make_unexpected("cipher not initialized");
            }

            if (ciphertext.size() < 12 + 16)
            {
                return nonstd::make_unexpected("ciphertext too short");
            }

            const uint8_t* iv = ciphertext.data();
            const uint8_t* enc_data = ciphertext.data() + 12;
            size_t enc_len = ciphertext.size() - 12 - 16;
            const uint8_t* tag = ciphertext.data() + ciphertext.size() - 16;

            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            if (!ctx)
            {
                LOG_ERROR("Failed to create cipher context");
                return nonstd::make_unexpected("failed to create cipher context");
            }

            if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
                EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
                EVP_DecryptInit_ex(ctx, nullptr, nullptr, impl_->aes_key.data(), iv) != 1)
            {
                EVP_CIPHER_CTX_free(ctx);
                LOG_ERROR("AES-GCM decrypt init failed");
                return nonstd::make_unexpected("AES-GCM decrypt init failed");
            }

            std::vector<uint8_t> plaintext(enc_len);
            int out_len = 0;
            if (EVP_DecryptUpdate(ctx, plaintext.data(), &out_len, enc_data, static_cast<int>(enc_len)) != 1)
            {
                EVP_CIPHER_CTX_free(ctx);
                LOG_ERROR("AES-GCM decrypt update failed");
                return nonstd::make_unexpected("AES-GCM decrypt update failed");
            }

            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t*>(tag)) != 1)
            {
                EVP_CIPHER_CTX_free(ctx);
                LOG_ERROR("AES-GCM set tag failed");
                return nonstd::make_unexpected("AES-GCM set tag failed");
            }

            int final_len = 0;
            if (EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len) != 1)
            {
                EVP_CIPHER_CTX_free(ctx);
                LOG_ERROR("AES-GCM decrypt final failed: authentication tag mismatch");
                return nonstd::make_unexpected("AES-GCM decrypt final failed: authentication tag mismatch");
            }

            EVP_CIPHER_CTX_free(ctx);
            plaintext.resize(static_cast<size_t>(out_len + final_len));
            return plaintext;
        }
    } // namespace crypto
} // namespace tyke
