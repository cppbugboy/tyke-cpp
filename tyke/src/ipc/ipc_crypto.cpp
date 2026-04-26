#include "ipc/ipc_crypto.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include "common/log_def.h"
#include "common/tyke_def.h"

namespace tyke::crypto
{
    struct EvpKdfDeleter
    {
        void operator()(EVP_KDF* kdf) const
        {
            EVP_KDF_free(kdf);
        }
    };

    using EvpKdfPtr = std::unique_ptr<EVP_KDF, EvpKdfDeleter>;

    struct EvpKdfCtxDeleter
    {
        void operator()(EVP_KDF_CTX* ctx) const
        {
            EVP_KDF_CTX_free(ctx);
        }
    };

    using EvpKdfCtxPtr = std::unique_ptr<EVP_KDF_CTX, EvpKdfCtxDeleter>;

    static ByteVecResult HkdfDeriveKey(const std::vector<uint8_t>& salt, const std::vector<uint8_t>& ikm,
                                       const std::vector<uint8_t>& info, size_t length)
    {
        EvpKdfPtr kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
        if (!kdf)
        {
            LOG_ERROR("EVP_KDF_fetch for HKDF failed");
            return nonstd::make_unexpected("EVP_KDF_fetch for HKDF failed");
        }

        EvpKdfCtxPtr ctx(EVP_KDF_CTX_new(kdf.get()));
        if (!ctx)
        {
            LOG_ERROR("EVP_KDF_CTX_new failed");
            return nonstd::make_unexpected("EVP_KDF_CTX_new failed");
        }

        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0),
            OSSL_PARAM_construct_octet_string("salt", const_cast<uint8_t*>(salt.data()), salt.size()),
            OSSL_PARAM_construct_octet_string("key", const_cast<uint8_t*>(ikm.data()), ikm.size()),
            OSSL_PARAM_construct_octet_string("info", const_cast<uint8_t*>(info.data()), info.size()),
            OSSL_PARAM_END
        };

        std::vector<uint8_t> out(length);
        if (EVP_KDF_derive(ctx.get(), out.data(), length, params) <= 0)
        {
            LOG_ERROR("EVP_KDF_derive failed");
            return nonstd::make_unexpected("EVP_KDF_derive failed");
        }

        LOG_DEBUG("HKDF key derivation via EVP_KDF completed, length={}", length);
        return out;
    }

    struct EvpCipherCtxDeleter
    {
        void operator()(EVP_CIPHER_CTX* ctx) const
        {
            EVP_CIPHER_CTX_free(ctx);
        }
    };

    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter>;

    struct EvpPkeyDeleter
    {
        void operator()(EVP_PKEY* pkey) const
        {
            EVP_PKEY_free(pkey);
        }
    };

    using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

    struct EvpPkeyCtxDeleter
    {
        void operator()(EVP_PKEY_CTX* ctx) const
        {
            EVP_PKEY_CTX_free(ctx);
        }
    };

    using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;

    static void EncodeLe32Impl(const uint32_t val, std::vector<uint8_t>& out)
    {
        out.push_back(static_cast<uint8_t>(val & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    }

    static uint32_t DecodeLe32Impl(const uint8_t* data)
    {
        return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
            (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
    }

    void FrameParser::EncodeLe32(const uint32_t val, std::vector<uint8_t>& out)
    {
        EncodeLe32Impl(val, out);
    }

    uint32_t FrameParser::DecodeLe32(const uint8_t* data)
    {
        return DecodeLe32Impl(data);
    }

    std::vector<uint8_t> FrameParser::BuildFrame(const uint8_t type, const std::vector<uint8_t>& payload)
    {
        std::vector<uint8_t> frame;
        const uint32_t total_len = 1 + static_cast<uint32_t>(payload.size());
        EncodeLe32Impl(total_len, frame);
        frame.push_back(type);
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    BoolResult FrameParser::ExtractFrame(std::vector<uint8_t>& buffer, uint8_t& type, std::vector<uint8_t>& payload)
    {
        if (buffer.size() < 5)
            return nonstd::make_unexpected("buffer too small for frame header");

        const uint32_t total_len = DecodeLe32Impl(buffer.data());
        if (total_len > kMaxFramePayloadLen)
        {
            LOG_ERROR("Frame payload too large: {} > {}, discarding buffer", total_len, kMaxFramePayloadLen);
            buffer.clear();
            return nonstd::make_unexpected("frame payload too large");
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

    struct EcdhKeyExchange::Impl
    {
        EvpPkeyPtr pkey;

        ~Impl()
        {
            if (pkey)
            {
                size_t key_len = 0;
                if (EVP_PKEY_get_octet_string_param(pkey.get(), OSSL_PKEY_PARAM_PUB_KEY,
                                                    nullptr, 0, &key_len) && key_len > 0)
                {
                    std::vector<unsigned char> buf(key_len);
                    if (EVP_PKEY_get_octet_string_param(pkey.get(), OSSL_PKEY_PARAM_PUB_KEY,
                                                        buf.data(), buf.size(), &key_len))
                    {
                        OPENSSL_cleanse(buf.data(), buf.size());
                    }
                }
                if (EVP_PKEY_get_octet_string_param(pkey.get(), OSSL_PKEY_PARAM_PRIV_KEY,
                                                    nullptr, 0, &key_len) && key_len > 0)
                {
                    std::vector<unsigned char> buf(key_len);
                    if (EVP_PKEY_get_octet_string_param(pkey.get(), OSSL_PKEY_PARAM_PRIV_KEY,
                                                        buf.data(), buf.size(), &key_len))
                    {
                        OPENSSL_cleanse(buf.data(), buf.size());
                    }
                }
            }
        }
    };

    EcdhKeyExchange::EcdhKeyExchange() : impl_(new Impl())
    {
    }

    EcdhKeyExchange::~EcdhKeyExchange() = default;

    BoolResult EcdhKeyExchange::GenerateKey()
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
        if (peer_pub_der.size() > static_cast<size_t>(LONG_MAX))
            return nonstd::make_unexpected("peer public key DER too large");
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

        if (EVP_PKEY_derive_init(ctx.get()) <= 0 || EVP_PKEY_derive_set_peer(ctx.get(), peer_pkey.get()) <= 0)
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
        std::atomic<bool> initialized{false};
        std::atomic<uint64_t> iv_counter{0};

        ~Impl()
        {
            if (!aes_key.empty())
            {
                OPENSSL_cleanse(aes_key.data(), aes_key.size());
            }
        }
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

        std::vector<uint8_t> salt = {
            't', 'y', 'k', 'e', '-', 'v', '1', '-', 'h', 'k', 'd', 'f', '-', 's', 'a', 'l', 't'
        };
        std::vector<uint8_t> info = {
            't', 'y', 'k', 'e', '-', 'v', '1', '-', 'a', 'e', 's', '2', '5', '6', '-', 'k', 'e', 'y'
        };

        auto key_result = HkdfDeriveKey(salt, shared_secret, info, kAes256KeyLen);
        if (!key_result)
        {
            LOG_ERROR("HKDF derive key failed: {}", key_result.error());
            return nonstd::make_unexpected("HKDF derive key failed: " + key_result.error());
        }

        if (key_result.value().size() != kAes256KeyLen)
        {
            LOG_ERROR("HKDF derive key length mismatch");
            return nonstd::make_unexpected("HKDF derive key length mismatch");
        }

        impl_->aes_key = std::move(key_result.value());
        impl_->initialized.store(true, std::memory_order_release);
        LOG_DEBUG("AES-GCM cipher initialized with HKDF");
        return true;
    }

    bool AesGcmCipher::IsInitialized() const
    {
        return impl_->initialized.load(std::memory_order_acquire);
    }

    ByteVecResult AesGcmCipher::Encrypt(const std::vector<uint8_t>& plaintext) const
    {
        if (!impl_->initialized.load(std::memory_order_acquire))
        {
            return nonstd::make_unexpected("cipher not initialized");
        }

        if (plaintext.size() > static_cast<size_t>(INT_MAX))
        {
            return nonstd::make_unexpected("plaintext too large for single encryption");
        }

        std::vector<uint8_t> iv(kAesGcmIvLen, 0);
        if (RAND_bytes(iv.data(), 4) != 1)
        {
            LOG_ERROR("RAND_bytes for IV prefix failed");
            return nonstd::make_unexpected("RAND_bytes for IV prefix failed");
        }
        const uint64_t counter = impl_->iv_counter.fetch_add(1, std::memory_order_relaxed);
        iv[4] = static_cast<uint8_t>((counter >> 56) & 0xFF);
        iv[5] = static_cast<uint8_t>((counter >> 48) & 0xFF);
        iv[6] = static_cast<uint8_t>((counter >> 40) & 0xFF);
        iv[7] = static_cast<uint8_t>((counter >> 32) & 0xFF);
        iv[8] = static_cast<uint8_t>((counter >> 24) & 0xFF);
        iv[9] = static_cast<uint8_t>((counter >> 16) & 0xFF);
        iv[10] = static_cast<uint8_t>((counter >> 8) & 0xFF);
        iv[11] = static_cast<uint8_t>(counter & 0xFF);

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
        if (!impl_->initialized.load(std::memory_order_acquire))
        {
            return nonstd::make_unexpected("cipher not initialized");
        }

        if (ciphertext.size() < kAesGcmIvLen + kAesGcmTagLen)
        {
            return nonstd::make_unexpected("ciphertext too short");
        }

        const size_t enc_len = ciphertext.size() - kAesGcmIvLen - kAesGcmTagLen;
        if (enc_len > static_cast<size_t>(INT_MAX))
        {
            return nonstd::make_unexpected("ciphertext too large for single decryption");
        }

        const uint8_t* iv = ciphertext.data();
        const uint8_t* enc_data = ciphertext.data() + kAesGcmIvLen;
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
} // namespace tyke::crypto
