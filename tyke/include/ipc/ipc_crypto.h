/**
 * @file ipc_crypto.h
 * @brief IPC加密模块声明。提供ECDH密钥交换和AES-GCM加解密功能。
 * @author Nick
 * @date 2026/04/19
 */


#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "common/tyke_result.h"

namespace tyke::crypto
{

        constexpr uint8_t kMsgHandshakeInit = 0x01;

        constexpr uint8_t kMsgHandshakeResp = 0x02;

        constexpr uint8_t kMsgData = 0x03;


        class FrameParser
        {
        public:

            static std::vector<uint8_t> BuildFrame(uint8_t type, const std::vector<uint8_t>& payload);


            static BoolResult ExtractFrame(std::vector<uint8_t>& buffer, uint8_t& type, std::vector<uint8_t>& payload);
        };


        class EcdhKeyExchange
        {
        public:

            EcdhKeyExchange();


            ~EcdhKeyExchange();


            BoolResult GenerateKey() const;


            ByteVecResult GetPublicKeyDer() const;


            ByteVecResult ComputeSharedSecret(const std::vector<uint8_t>& peer_pub_der) const;

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;
        };


        class AesGcmCipher
        {
        public:

            AesGcmCipher();


            ~AesGcmCipher();


            BoolResult Init(const std::vector<uint8_t>& shared_secret) const;


            bool IsInitialized() const;


            ByteVecResult Encrypt(const std::vector<uint8_t>& plaintext) const;


            ByteVecResult Decrypt(const std::vector<uint8_t>& ciphertext) const;

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;
        };
}
