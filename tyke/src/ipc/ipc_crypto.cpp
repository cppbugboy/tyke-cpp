#include "ipc/ipc_crypto.h"
#include "common/log_def.h"
#include "common/tyke_def.h"

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/rand.h>

namespace tyke::crypto
{
        struct EvpCipherCtxDeleter
        {
            void operator()(EVP_CIPHER_CTX* ctx) const { EVP_CIPHER_CTX_free(ctx); }
        };
        using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter>;

        struct EvpPkeyDeleter
        {
            void operator()(EVP_PKEY* pkey) const { EVP_PKEY_free(pkey); }
        };
        using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

        struct EvpPkeyCtxDeleter
        {
            void operator()(EVP_PKEY_CTX* ctx) const { EVP_PKEY_CTX_free(ctx); }
        };
        using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;

        static void EncodeU32(const uint32_t val, std::vector<uint8_t>& out)
        {
            out.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
            out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>(val & 0xFF));
        }

        static uint32_t DecodeU32(const uint8_t* data)
        {
            return (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8) |
                   static_cast<uint32_t>(data[3]);
        }

        std::vector<uint8_t> FrameParser::BuildFrame(const uint8_t type, const std::vector<uint8_t>& payload)
        {
            std::vector<uint8_t> frame;
            const uint32_t total_len = 1 + static_cast<uint32_t>(payload.size());
            EncodeU32(total_len, frame);
            frame.push_back(type);
            frame.insert(frame.end(), payload.begin(), payload.end());
            return frame;
        }

        BoolResult FrameParser::ExtractFrame(std::vector<uint8_t>& buffer, uint8_t& type, std::vector<uint8_t>& payload)
        {
            if (buffer.size() < 5)
                return nonstd::make_unexpected("buffer too small for frame header");

            const uint32_t total_len = DecodeU32(buffer.data());
            if (buffer.size() < 4 + total_len)
                return nonstd::make_unexpected("buffer incomplete: expected " +
                    std::to_string(4 + total_len) + " bytes, got " + std::to_string(buffer.size()));

            type = buffer[4];
            payload.assign(buffer.begin() + 5, buffer.begin() + 4 + total_len);
            buffer.erase(buffer.begin(), buffer.begin() + 4 + total_len);
            return true;
        }

        struct EcdhKeyExchange::Impl
        {
            EvpPkeyPtr pkey;
        };

        EcdhKeyExchange::EcdhKeyExchange() : impl_(new Impl())
        {
        }

        EcdhKeyExchange::~EcdhKeyExchange()
        = default;

        BoolResult EcdhKeyExchange::GenerateKey() const
        {
            impl_->pkey.reset();

            EVP_PKEY* raw_pkey = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1");
            if (!raw_pkey)
            {
                LOG_ERROR("ECDH key generation failed");
                return nonstd::make_unexpected("ECDH key generation failed");
            }
            impl_->pkey.reset(raw_pkey);
            LOG_DEBUG("ECDH key generated successfully");
            return true;
        }

        ByteVecResult EcdhKeyExchange::GetPublicKeyDer() const
        {
            if (!impl_->pkey)
            {
                return nonstd::make_unexpected("no ECDH key available");
            }

            const int len = i2d_PUBKEY(impl_->pkey.get(), nullptr);
            if (len <= 0)
            {
                LOG_ERROR("Failed to get public key DER length");
                return nonstd::make_unexpected("failed to get public key DER length");
            }

            std::vector<uint8_t> der(static_cast<size_t>(len));
            uint8_t* ptr = der.data();
            if (i2d_PUBKEY(impl_->pkey.get(), &ptr) <= 0)
            {
                LOG_ERROR("Failed to export public key DER");
                return nonstd::make_unexpected("failed to export public key DER");
            }
            return der;
        }

        ByteVecResult EcdhKeyExchange::ComputeSharedSecret(const std::vector<uint8_t>& peer_pub_der) const
        {
            if (!impl_->pkey)
            {
                return nonstd::make_unexpected("no ECDH key available");
            }

            const uint8_t* ptr = peer_pub_der.data();
            const EvpPkeyPtr peer_pkey(d2i_PUBKEY(nullptr, &ptr, static_cast<long>(peer_pub_der.size())));
            if (!peer_pkey)
            {
                LOG_ERROR("Failed to parse peer public key DER");
                return nonstd::make_unexpected("failed to parse peer public key DER");
            }

            const EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new(impl_->pkey.get(), nullptr));
            if (!ctx)
            {
                LOG_ERROR("Failed to create PKEY context");
                return nonstd::make_unexpected("failed to create PKEY context");
            }

            if (EVP_PKEY_derive_init(ctx.get()) <= 0 ||
                EVP_PKEY_derive_set_peer(ctx.get(), peer_pkey.get()) <= 0)
            {
                LOG_ERROR("Failed to initialize ECDH derivation");
                return nonstd::make_unexpected("failed to initialize ECDH derivation");
            }

            size_t secret_len = 0;
            if (EVP_PKEY_derive(ctx.get(), nullptr, &secret_len) <= 0)
            {
                LOG_ERROR("Failed to determine shared secret length");
                return nonstd::make_unexpected("failed to determine shared secret length");
            }

            std::vector<uint8_t> secret(secret_len);
            if (EVP_PKEY_derive(ctx.get(), secret.data(), &secret_len) <= 0)
            {
                LOG_ERROR("Failed to compute shared secret");
                return nonstd::make_unexpected("failed to compute shared secret");
            }

            LOG_DEBUG("Shared secret computed successfully, length={}", secret_len);
            return secret;
        }

        struct AesGcmCipher::Impl
        {
            std::vector<uint8_t> aes_key;
            bool initialized = false;
        };

        AesGcmCipher::AesGcmCipher() : impl_(new Impl())
        {
        }

        AesGcmCipher::~AesGcmCipher()
        {
        }

        BoolResult AesGcmCipher::Init(const std::vector<uint8_t>& shared_secret) const
        {
            if (shared_secret.empty())
            {
                return nonstd::make_unexpected("shared secret is empty");
            }

            impl_->aes_key.resize(kAes256KeyLen);
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

        ByteVecResult AesGcmCipher::Encrypt(const std::vector<uint8_t>& plaintext) const
        {
            if (!impl_->initialized)
            {
                return nonstd::make_unexpected("cipher not initialized");
            }

            std::vector<uint8_t> iv(kAesGcmIvLen);
            if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1)
            {
                LOG_ERROR("RAND_bytes for IV failed");
                return nonstd::make_unexpected("RAND_bytes for IV failed");
            }

            const EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
            if (!ctx)
            {
                LOG_ERROR("Failed to create cipher context");
                return nonstd::make_unexpected("failed to create cipher context");
            }

            if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
                EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, kAesGcmIvLen, nullptr) != 1 ||
                EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, impl_->aes_key.data(), iv.data()) != 1)
            {
                LOG_ERROR("AES-GCM encrypt init failed");
                return nonstd::make_unexpected("AES-GCM encrypt init failed");
            }

            std::vector<uint8_t> ciphertext(plaintext.size());
            int out_len = 0;
            if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &out_len, plaintext.data(),
                                  static_cast<int>(plaintext.size())) != 1)
            {
                LOG_ERROR("AES-GCM encrypt update failed");
                return nonstd::make_unexpected("AES-GCM encrypt update failed");
            }

            int final_len = 0;
            if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + out_len, &final_len) != 1)
            {
                LOG_ERROR("AES-GCM encrypt final failed");
                return nonstd::make_unexpected("AES-GCM encrypt final failed");
            }

            std::vector<uint8_t> tag(kAesGcmTagLen);
            if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, kAesGcmTagLen, tag.data()) != 1)
            {
                LOG_ERROR("AES-GCM get tag failed");
                return nonstd::make_unexpected("AES-GCM get tag failed");
            }

            std::vector<uint8_t> result;
            result.reserve(kAesGcmIvLen + ciphertext.size() + kAesGcmTagLen);
            result.insert(result.end(), iv.begin(), iv.end());
            result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + out_len + final_len);
            result.insert(result.end(), tag.begin(), tag.end());
            return result;
        }

        ByteVecResult AesGcmCipher::Decrypt(const std::vector<uint8_t>& ciphertext) const
        {
            if (!impl_->initialized)
            {
                return nonstd::make_unexpected("cipher not initialized");
            }

            if (ciphertext.size() < kAesGcmIvLen + kAesGcmTagLen)
            {
                return nonstd::make_unexpected("ciphertext too short");
            }

            const uint8_t* iv = ciphertext.data();
            const uint8_t* enc_data = ciphertext.data() + kAesGcmIvLen;
            const size_t enc_len = ciphertext.size() - kAesGcmIvLen - kAesGcmTagLen;
            const uint8_t* tag = ciphertext.data() + ciphertext.size() - kAesGcmTagLen;

            const EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
            if (!ctx)
            {
                LOG_ERROR("Failed to create cipher context");
                return nonstd::make_unexpected("failed to create cipher context");
            }

            if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
                EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, kAesGcmIvLen, nullptr) != 1 ||
                EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, impl_->aes_key.data(), iv) != 1)
            {
                LOG_ERROR("AES-GCM decrypt init failed");
                return nonstd::make_unexpected("AES-GCM decrypt init failed");
            }

            std::vector<uint8_t> plaintext(enc_len);
            int out_len = 0;
            if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &out_len, enc_data, static_cast<int>(enc_len)) != 1)
            {
                LOG_ERROR("AES-GCM decrypt update failed");
                return nonstd::make_unexpected("AES-GCM decrypt update failed");
            }

            if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, kAesGcmTagLen, const_cast<uint8_t*>(tag)) != 1)
            {
                LOG_ERROR("AES-GCM set tag failed");
                return nonstd::make_unexpected("AES-GCM set tag failed");
            }

            int final_len = 0;
            if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + out_len, &final_len) != 1)
            {
                LOG_ERROR("AES-GCM decrypt final failed: authentication tag mismatch");
                return nonstd::make_unexpected("AES-GCM decrypt final failed: authentication tag mismatch");
            }

            plaintext.resize(static_cast<size_t>(out_len + final_len));
            return plaintext;
        }
}
