#ifdef _WIN32
#include "common/log_def.h"
#include "common/tyke_utils.h"
#include "component/thread_pool.h"
#include "ipc/ipc_crypto.h"
#include "ipc/ipc_internal_platform.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <windows.h>

namespace tyke
{
    class ClientConnectionImplWin : public IClientConnectionImpl
    {
    public:
        ClientConnectionImplWin()
        {
            event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        }

        ~ClientConnectionImplWin() override
        {
            ClientConnectionImplWin::Close();
        }

        BoolResult Connect(std::string_view server_name, const uint32_t timeout_ms, uint32_t rw_timeout_ms) override
        {
            LOG_INFO("ipc client connecting to: {}", server_name);
            if (!utils::IsValidServerName(server_name))
                return nonstd::make_unexpected("invalid server name");
            if (!event_)
            {
                event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
                if (!event_)
                    return nonstd::make_unexpected("failed to create event handle");
            }
            const std::string name = std::string(R"(\\.\pipe\)") + std::string(server_name);
            if (!WaitNamedPipeA(name.c_str(), timeout_ms))
                return nonstd::make_unexpected(std::string("WaitNamedPipe failed for: ") + std::string(server_name));
            pipe_ = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                FILE_FLAG_OVERLAPPED,
                                nullptr);
            if (pipe_ == INVALID_HANDLE_VALUE)
                return nonstd::make_unexpected(std::string("CreateFile failed for pipe: ") + std::string(server_name));
            DWORD mode = PIPE_READMODE_BYTE;
            SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr);
            if (rw_timeout_ms > 0)
            {
                DWORD rw_timeout = rw_timeout_ms;
                SetNamedPipeHandleState(pipe_, nullptr, &rw_timeout, nullptr);
            }
            auto handshake_result = DoHandshake(rw_timeout_ms > 0 ? rw_timeout_ms : timeout_ms);
            if (!handshake_result)
            {
                CancelIoEx(pipe_, nullptr);
                CloseHandle(pipe_);
                pipe_ = INVALID_HANDLE_VALUE;
                return handshake_result;
            }
            return true;
        }

        BoolResult WriteEncrypted(const void* data, const size_t size, const uint32_t timeout_ms) override
        {
            if (size <= crypto::kFragmentChunkSize)
            {
                const std::vector<uint8_t> pt(static_cast<const uint8_t*>(data),
                                              static_cast<const uint8_t*>(data) + size);
                auto encrypt_result = cipher_.Encrypt(pt);
                if (!encrypt_result)
                    return nonstd::make_unexpected("encrypt failed: " + encrypt_result.error());
                const auto frame = crypto::FrameParser::BuildFrame(crypto::kMsgData, encrypt_result.value());
                return WriteExact(frame.data(), frame.size(), timeout_ms);
            }

            const auto* ptr = static_cast<const uint8_t*>(data);
            size_t remaining = size;
            size_t offset = 0;
            while (remaining > 0)
            {
                const size_t chunk_size = std::min(remaining, static_cast<size_t>(crypto::kFragmentChunkSize));
                std::vector<uint8_t> fragment_payload;
                crypto::FrameParser::EncodeLe32(static_cast<uint32_t>(size), fragment_payload);
                crypto::FrameParser::EncodeLe32(static_cast<uint32_t>(offset), fragment_payload);
                std::vector<uint8_t> chunk(ptr, ptr + chunk_size);
                auto encrypt_result = cipher_.Encrypt(chunk);
                if (!encrypt_result)
                    return nonstd::make_unexpected("encrypt fragment failed: " + encrypt_result.error());
                fragment_payload.insert(fragment_payload.end(), encrypt_result.value().begin(),
                                        encrypt_result.value().end());
                auto frame = crypto::FrameParser::BuildFrame(crypto::kMsgDataFragment, fragment_payload);
                auto write_result = WriteExact(frame.data(), frame.size(), timeout_ms);
                if (!write_result)
                    return write_result;
                ptr += chunk_size;
                offset += chunk_size;
                remaining -= chunk_size;
            }
            return true;
        }

        BoolResult ReadLoop(const ClientRecvDataCallback& callback, const uint32_t timeout_ms) override
        {
            std::vector<uint8_t> raw_buf;
            std::vector<uint8_t> plain_buf;
            uint8_t chunk[131072];
            while (true)
            {
                ResetEvent(event_);
                OVERLAPPED ov{};
                ov.hEvent = event_;
                DWORD bytes_read = 0;
                if (const BOOL result = ReadFile(pipe_, chunk, sizeof(chunk), &bytes_read, &ov); !result)
                {
                    if (const DWORD error = GetLastError(); error == ERROR_IO_PENDING || error == ERROR_MORE_DATA)
                    {
                        if (WaitForSingleObject(ov.hEvent, timeout_ms) != WAIT_OBJECT_0)
                            return nonstd::make_unexpected("read loop: wait timeout");
                        if (!GetOverlappedResult(pipe_, &ov, &bytes_read, FALSE))
                        {
                            if (GetLastError() != ERROR_MORE_DATA)
                                return nonstd::make_unexpected("read loop: GetOverlappedResult failed");
                        }
                    }
                    else
                    {
                        return nonstd::make_unexpected(
                            "read loop: ReadFile failed with error " + std::to_string(error));
                    }
                }
                if (bytes_read == 0)
                    break;
                raw_buf.insert(raw_buf.end(), chunk, chunk + bytes_read);
                uint8_t type = 0;
                std::vector<uint8_t> payload;
                while (crypto::FrameParser::ExtractFrame(raw_buf, type, payload))
                {
                    if (type == crypto::kMsgData)
                    {
                        auto decrypt_result = cipher_.Decrypt(payload);
                        if (!decrypt_result)
                            return nonstd::make_unexpected("read loop decrypt failed: " + decrypt_result.error());
                        plain_buf.insert(plain_buf.end(), decrypt_result.value().begin(), decrypt_result.value().end());
                    }
                    else if (type == crypto::kMsgDataFragment)
                    {
                        auto reassemble_result = ReassembleFragment(payload);
                        if (!reassemble_result)
                            return nonstd::make_unexpected("read loop fragment failed: " + reassemble_result.error());
                        if (reassemble_result.value())
                        {
                            plain_buf.insert(plain_buf.end(), reassembly_buf_.begin(), reassembly_buf_.end());
                            reassembly_buf_.clear();
                            reassembly_total_ = 0;
                            reassembly_received_ = 0;
                        }
                    }
                }
                if (!plain_buf.empty())
                {
                    if (callback(plain_buf))
                        return true;
                }
            }
            return nonstd::make_unexpected("read loop: connection closed");
        }

        void Close() override
        {
            LOG_INFO("ipc client closing connection");
            if (pipe_ != INVALID_HANDLE_VALUE)
            {
                CancelIoEx(pipe_, nullptr);
                CloseHandle(pipe_);
                pipe_ = INVALID_HANDLE_VALUE;
            }
            if (event_)
            {
                CloseHandle(event_);
                event_ = nullptr;
            }
            reassembly_buf_.clear();
            reassembly_total_ = 0;
            reassembly_received_ = 0;
        }

        bool IsValid() const override
        {
            return pipe_ != INVALID_HANDLE_VALUE && event_ != nullptr && cipher_.IsInitialized();
        }

    private:
        BoolResult WriteExact(const void* data, size_t size, uint32_t timeout_ms) const
        {
            auto ptr = static_cast<const uint8_t*>(data);
            size_t remaining = size;
            while (remaining > 0)
            {
                ResetEvent(event_);
                OVERLAPPED ov{};
                ov.hEvent = event_;
                DWORD written = 0;
                const BOOL result = WriteFile(pipe_, ptr, static_cast<DWORD>(remaining), &written, &ov);
                if (!result)
                {
                    if (GetLastError() == ERROR_IO_PENDING)
                    {
                        if (WaitForSingleObject(ov.hEvent, timeout_ms) != WAIT_OBJECT_0)
                            return nonstd::make_unexpected("write exact: wait timeout");
                        if (!GetOverlappedResult(pipe_, &ov, &written, FALSE))
                            return nonstd::make_unexpected("write exact: GetOverlappedResult failed");
                    }
                    else
                    {
                        return nonstd::make_unexpected("write exact: WriteFile failed with error " +
                            std::to_string(GetLastError()));
                    }
                }
                if (written == 0)
                    return nonstd::make_unexpected("write exact: wrote zero bytes");
                ptr += written;
                remaining -= written;
            }
            return true;
        }

        BoolResult ReassembleFragment(const std::vector<uint8_t>& payload)
        {
            if (payload.size() < crypto::kFragmentHeaderSize)
                return nonstd::make_unexpected("fragment payload too small");
            const uint32_t total_size = crypto::FrameParser::DecodeLe32(payload.data());
            const uint32_t offset = crypto::FrameParser::DecodeLe32(payload.data() + 4);
            std::vector<uint8_t> encrypted_chunk(payload.begin() + crypto::kFragmentHeaderSize, payload.end());
            auto decrypt_result = cipher_.Decrypt(encrypted_chunk);
            if (!decrypt_result)
                return nonstd::make_unexpected("decrypt fragment failed: " + decrypt_result.error());
            if (offset == 0)
            {
                reassembly_buf_.resize(total_size);
                reassembly_total_ = total_size;
                reassembly_received_ = 0;
            }
            if (static_cast<size_t>(offset) + decrypt_result.value().size() > reassembly_total_)
                return nonstd::make_unexpected("fragment offset overflow");
            std::memcpy(reassembly_buf_.data() + offset, decrypt_result.value().data(),
                        decrypt_result.value().size());
            reassembly_received_ += static_cast<uint32_t>(decrypt_result.value().size());
            return reassembly_received_ == reassembly_total_;
        }

        BoolResult DoHandshake(uint32_t timeout) const
        {
            crypto::EcdhKeyExchange ecdh;
            if (auto gen_result = ecdh.GenerateKey(); !gen_result)
                return nonstd::make_unexpected("handshake: key generation failed: " + gen_result.error());

            auto pub_der_result = ecdh.GetPublicKeyDer();
            if (!pub_der_result)
                return nonstd::make_unexpected("handshake: get public key failed: " + pub_der_result.error());

            auto init_frame = crypto::FrameParser::BuildFrame(crypto::kMsgHandshakeInit, pub_der_result.value());
            if (auto write_result = WriteExact(init_frame.data(), init_frame.size(), timeout); !write_result)
                return nonstd::make_unexpected("handshake: write init frame failed: " + write_result.error());

            std::vector<uint8_t> raw_buf;
            uint8_t chunk[1024];
            uint8_t type = 0;
            std::vector<uint8_t> payload;

            while (true)
            {
                ResetEvent(event_);
                OVERLAPPED ov{};
                ov.hEvent = event_;
                DWORD bytes = 0;
                if (!ReadFile(pipe_, chunk, sizeof(chunk), &bytes, &ov))
                {
                    if (GetLastError() == ERROR_IO_PENDING || GetLastError() == ERROR_MORE_DATA)
                    {
                        if (WaitForSingleObject(ov.hEvent, timeout) != WAIT_OBJECT_0)
                            return nonstd::make_unexpected("handshake: read timeout");
                        if (!GetOverlappedResult(pipe_, &ov, &bytes, FALSE))
                            return nonstd::make_unexpected("handshake: GetOverlappedResult failed");
                    }
                    else
                    {
                        return nonstd::make_unexpected("handshake: ReadFile failed with error " +
                            std::to_string(GetLastError()));
                    }
                }
                if (bytes == 0)
                    return nonstd::make_unexpected("handshake: connection closed");
                raw_buf.insert(raw_buf.end(), chunk, chunk + bytes);
                if (auto extract_result = crypto::FrameParser::ExtractFrame(raw_buf, type, payload))
                {
                    if (type == crypto::kMsgHandshakeResp)
                    {
                        auto secret_result = ecdh.ComputeSharedSecret(payload);
                        if (!secret_result)
                            return nonstd::make_unexpected("handshake: compute shared secret failed: " +
                                secret_result.error());
                        if (auto init_result = cipher_.Init(secret_result.value()); !init_result)
                            return nonstd::make_unexpected("handshake: cipher init failed: " + init_result.error());
                        return true;
                    }
                    return nonstd::make_unexpected("handshake: unexpected frame type");
                }
            }
        }

        HANDLE pipe_ = INVALID_HANDLE_VALUE;
        HANDLE event_ = nullptr;
        crypto::AesGcmCipher cipher_;
        std::vector<uint8_t> reassembly_buf_;
        uint32_t reassembly_total_ = 0;
        uint32_t reassembly_received_ = 0;
    };

    class ServerImplWin : public IServerImpl
    {
        enum ClientState
        {
            STATE_WAIT_HELLO,
            STATE_ESTABLISHED
        };

        struct ClientContext
        {
            OVERLAPPED read_ov{};
            OVERLAPPED write_ov{};
            HANDLE pipe;
            ClientState state;
            crypto::EcdhKeyExchange ecdh;
            crypto::AesGcmCipher cipher;
            std::vector<uint8_t> raw_recv_buf;
            std::vector<uint8_t> pending_writes;
            bool connected;
            std::atomic<bool> writing{false};
            std::mutex write_mutex;
            uint8_t raw_read_buf[131072];
            std::vector<uint8_t> reassembly_buf;
            uint32_t reassembly_total = 0;
            uint32_t reassembly_received = 0;

            ClientContext()
                : pipe(INVALID_HANDLE_VALUE), state(STATE_WAIT_HELLO), connected(false), writing(false), raw_read_buf{}
            {
                memset(&read_ov, 0, sizeof(read_ov));
                memset(&write_ov, 0, sizeof(write_ov));
                read_ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
                write_ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            }

            ~ClientContext()
            {
                if (read_ov.hEvent)
                    CloseHandle(read_ov.hEvent);
                if (write_ov.hEvent)
                    CloseHandle(write_ov.hEvent);
            }
        };

    public:
        ServerImplWin() : running_(false), iocp_(nullptr)
        {
        }

        BoolResult Start(std::string_view server_name, ServerRecvDataCallback callback) override
        {
            LOG_INFO("ipc server starting on: {}", server_name);
            if (!utils::IsValidServerName(server_name))
                return nonstd::make_unexpected("invalid server name");
            if (running_.load())
                return nonstd::make_unexpected("server already running");
            callback_ = std::move(callback);
            server_name_ = std::string(R"(\\.\pipe\)") + std::string(server_name);
            iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
            if (!iocp_)
                return nonstd::make_unexpected("CreateIoCompletionPort failed");
            running_ = true;
            const int num_workers = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
            for (int i = 0; i < num_workers; i++)
            {
                workers_.emplace_back(&ServerImplWin::WorkerProc, this);
            }
            const int pre_create_count = std::max(4, num_workers);
            for (int i = 0; i < pre_create_count; i++)
            {
                if (auto result = CreateListeningPipe(); !result)
                {
                    LOG_WARN("Pre-create pipe {} failed: {}", i, result.error());
                }
            }
            return true;
        }

        void Stop() override
        {
            if (!running_.exchange(false))
                return;
            for (size_t i = 0; i < workers_.size() + 1; i++)
            {
                PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);
            }
            for (auto& w : workers_)
            {
                if (w.joinable())
                    w.join();
            }
            workers_.clear();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& [fst, snd] : clients_)
                {
                    DisconnectNamedPipe(snd->pipe);
                    CloseHandle(snd->pipe);
                }
                clients_.clear();
            }
            CloseHandle(iocp_);
        }

        BoolResult SendToClient(const ClientId id, const std::vector<uint8_t>& data) override
        {
            std::shared_ptr<ClientContext> ctx;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto it = clients_.find(id);
                if (it == clients_.end())
                    return nonstd::make_unexpected("client not found: " + std::to_string(id));
                ctx = it->second;
            }

            if (data.size() <= crypto::kFragmentChunkSize)
            {
                auto encrypt_result = ctx->cipher.Encrypt(data);
                if (!encrypt_result)
                    return nonstd::make_unexpected("encrypt failed for client " + std::to_string(id) + ": " +
                        encrypt_result.error());
                auto frame = crypto::FrameParser::BuildFrame(crypto::kMsgData, encrypt_result.value());
                std::lock_guard<std::mutex> lock(ctx->write_mutex);
                ctx->pending_writes.insert(ctx->pending_writes.end(), frame.begin(), frame.end());
                if (!ctx->writing)
                {
                    StartWrite(ctx);
                }
                return true;
            }

            size_t offset = 0;
            size_t remaining = data.size();
            std::lock_guard<std::mutex> lock(ctx->write_mutex);
            while (remaining > 0)
            {
                const size_t chunk_size = std::min(remaining, static_cast<size_t>(crypto::kFragmentChunkSize));
                std::vector<uint8_t> fragment_payload;
                crypto::FrameParser::EncodeLe32(static_cast<uint32_t>(data.size()), fragment_payload);
                crypto::FrameParser::EncodeLe32(static_cast<uint32_t>(offset), fragment_payload);
                std::vector<uint8_t> chunk(data.begin() + offset, data.begin() + offset + chunk_size);
                auto encrypt_result = ctx->cipher.Encrypt(chunk);
                if (!encrypt_result)
                    return nonstd::make_unexpected("encrypt fragment failed for client " + std::to_string(id));
                fragment_payload.insert(fragment_payload.end(), encrypt_result.value().begin(),
                                        encrypt_result.value().end());
                auto frame = crypto::FrameParser::BuildFrame(crypto::kMsgDataFragment, fragment_payload);
                ctx->pending_writes.insert(ctx->pending_writes.end(), frame.begin(), frame.end());
                offset += chunk_size;
                remaining -= chunk_size;
            }
            if (!ctx->writing)
            {
                StartWrite(ctx);
            }
            return true;
        }

    private:
        void WorkerProc()
        {
            while (running_)
            {
                DWORD bytes = 0;
                ULONG_PTR key = 0;
                OVERLAPPED* ov = nullptr;
                const BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &ov, INFINITE);
                if (!running_)
                    break;
                auto* ctx_raw = reinterpret_cast<ClientContext*>(key);
                if (!ctx_raw)
                    continue;

                std::shared_ptr<ClientContext> ctx;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = clients_.find(reinterpret_cast<ClientId>(ctx_raw->pipe));
                    if (it == clients_.end())
                        continue;
                    ctx = it->second;
                }

                if (!ok)
                {
                    if (ov == nullptr)
                        continue;
                    const DWORD iocp_err = GetLastError();
                    if (iocp_err == ERROR_OPERATION_ABORTED)
                        continue;
                    LOG_WARN("IOCP completion error: {}, closing client", iocp_err);
                    CloseClient(ctx);
                    continue;
                }

                if (bytes == 0 && ctx->connected)
                {
                    CloseClient(ctx);
                    continue;
                }

                if (!ctx->connected)
                {
                    ctx->connected = true;
                    LOG_INFO("Client connected, pipe={}", reinterpret_cast<uintptr_t>(ctx->pipe));
                    if (auto result = CreateListeningPipe(); !result)
                    {
                        LOG_WARN("CreateListeningPipe failed: {}", result.error());
                    }
                    PostRead(ctx);
                }
                else if (ov == &ctx->read_ov)
                {
                    if (bytes > 0)
                    {
                        ctx->raw_recv_buf.insert(ctx->raw_recv_buf.end(), ctx->raw_read_buf, ctx->raw_read_buf + bytes);
                        if (!ProcessFrames(ctx))
                        {
                            LOG_WARN("ProcessFrames failed, closing client, pipe={}",
                                     reinterpret_cast<uintptr_t>(ctx->pipe));
                            CloseClient(ctx);
                            continue;
                        }
                    }
                    else
                    {
                        LOG_INFO("Read completion with 0 bytes (client disconnected), pipe={}",
                                 reinterpret_cast<uintptr_t>(ctx->pipe));
                        CloseClient(ctx);
                    }
                }
                else if (ov == &ctx->write_ov)
                {
                    bool should_close = false;
                    {
                        std::lock_guard<std::mutex> lock(ctx->write_mutex);
                        ctx->pending_writes.erase(ctx->pending_writes.begin(), ctx->pending_writes.begin() + bytes);
                        ctx->writing = false;
                        if (!ctx->pending_writes.empty())
                        {
                            if (!StartWrite(ctx))
                                should_close = true;
                        }
                    }
                    if (should_close)
                    {
                        LOG_WARN("StartWrite failed after write completion, closing client");
                        CloseClient(ctx);
                    }
                }
            }
        }

        BoolResult CreateListeningPipe()
        {
            HANDLE pipe = CreateNamedPipeA(server_name_.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                           PIPE_TYPE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
                                           262144, 262144, 0, nullptr);
            if (pipe == INVALID_HANDLE_VALUE)
                return nonstd::make_unexpected("CreateNamedPipe failed");
            const auto ctx = std::make_shared<ClientContext>();
            ctx->pipe = pipe;
            if (!CreateIoCompletionPort(pipe, iocp_, reinterpret_cast<ULONG_PTR>(ctx.get()), 0))
            {
                CloseHandle(pipe);
                return nonstd::make_unexpected("CreateIoCompletionPort for pipe failed");
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                clients_[reinterpret_cast<ClientId>(pipe)] = ctx;
            }
            if (const BOOL pending = ConnectNamedPipe(pipe, &ctx->read_ov); !pending)
            {
                if (const DWORD err = GetLastError(); err == ERROR_PIPE_CONNECTED)
                {
                    PostQueuedCompletionStatus(iocp_, 0, reinterpret_cast<ULONG_PTR>(ctx.get()), &ctx->read_ov);
                }
                else if (err != ERROR_IO_PENDING)
                {
                    CloseClient(ctx);
                    return nonstd::make_unexpected("ConnectNamedPipe failed with error " + std::to_string(err));
                }
            }
            return true;
        }

        void PostRead(const std::shared_ptr<ClientContext>& ctx)
        {
            const HANDLE hEvent = ctx->read_ov.hEvent;
            memset(&ctx->read_ov, 0, sizeof(ctx->read_ov));
            ctx->read_ov.hEvent = hEvent;
            DWORD bytes = 0;
            if (const BOOL ok = ReadFile(ctx->pipe, ctx->raw_read_buf, sizeof(ctx->raw_read_buf), &bytes,
                                         &ctx->read_ov);
                !ok)
            {
                const DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING || err == ERROR_MORE_DATA)
                    return;
                if (err == ERROR_OPERATION_ABORTED || err == ERROR_INVALID_HANDLE)
                    return;
                LOG_WARN("PostRead ReadFile failed, pipe={}, err={}", reinterpret_cast<uintptr_t>(ctx->pipe), err);
                CloseClient(ctx);
            }
        }

        bool ProcessFrames(const std::shared_ptr<ClientContext>& ctx)
        {
            uint8_t type = 0;
            std::vector<uint8_t> payload;
            while (crypto::FrameParser::ExtractFrame(ctx->raw_recv_buf, type, payload))
            {
                if (ctx->state == STATE_WAIT_HELLO)
                {
                    if (type != crypto::kMsgHandshakeInit)
                    {
                        LOG_WARN("ProcessFrames: expected handshake init, got type={}", static_cast<int>(type));
                        return false;
                    }
                    if (auto gen_result = ctx->ecdh.GenerateKey(); !gen_result)
                    {
                        LOG_WARN("ProcessFrames: ECDH key generation failed");
                        return false;
                    }
                    auto secret_result = ctx->ecdh.ComputeSharedSecret(payload);
                    if (!secret_result)
                    {
                        LOG_WARN("ProcessFrames: shared secret computation failed");
                        return false;
                    }
                    if (auto init_result = ctx->cipher.Init(secret_result.value()); !init_result)
                    {
                        LOG_WARN("ProcessFrames: cipher init failed");
                        return false;
                    }
                    auto pub_der_result = ctx->ecdh.GetPublicKeyDer();
                    if (!pub_der_result)
                    {
                        LOG_WARN("ProcessFrames: public key derivation failed");
                        return false;
                    }
                    auto resp = crypto::FrameParser::BuildFrame(crypto::kMsgHandshakeResp, pub_der_result.value());

                    std::lock_guard<std::mutex> lock(ctx->write_mutex);
                    ctx->pending_writes.insert(ctx->pending_writes.end(), resp.begin(), resp.end());
                    if (!ctx->writing)
                        StartWrite(ctx);
                    ctx->state = STATE_ESTABLISHED;
                }
                else if (ctx->state == STATE_ESTABLISHED)
                {
                    if (type == crypto::kMsgData)
                    {
                        auto decrypt_result = ctx->cipher.Decrypt(payload);
                        if (!decrypt_result)
                        {
                            LOG_WARN("ProcessFrames: data decryption failed, payload_size={}", payload.size());
                            return false;
                        }

                        auto data_copy = std::make_shared<std::vector<uint8_t>>(std::move(decrypt_result.value()));
                        auto client_id = reinterpret_cast<ClientId>(ctx->pipe);
                        auto callback = callback_;

                        auto enqueue_result = GetGlobalThreadPool().Enqueue(
                            [callback, client_id, data_copy, this]()
                            {
                                auto cb_send = [this](const ClientId id, const std::vector<uint8_t>& buf) -> bool
                                {
                                    const auto result = SendToClient(id, buf);
                                    return result.has_value();
                                };
                                if (callback)
                                {
                                    if (const auto optional = callback(client_id, *data_copy, cb_send); !optional)
                                    {
                                        CloseClient(client_id);
                                    }
                                }
                            });
                        if (!enqueue_result)
                        {
                            LOG_WARN("Thread pool stopped, cannot process data for client_id={}", client_id);
                        }
                    }
                    else if (type == crypto::kMsgDataFragment)
                    {
                        if (payload.size() < crypto::kFragmentHeaderSize)
                        {
                            LOG_WARN("ProcessFrames: fragment payload too small, size={}", payload.size());
                            return false;
                        }
                        const uint32_t total_size = crypto::FrameParser::DecodeLe32(payload.data());
                        const uint32_t offset = crypto::FrameParser::DecodeLe32(payload.data() + 4);
                        std::vector<uint8_t> encrypted_chunk(payload.begin() + crypto::kFragmentHeaderSize,
                                                             payload.end());
                        auto decrypt_result = ctx->cipher.Decrypt(encrypted_chunk);
                        if (!decrypt_result)
                        {
                            LOG_WARN("ProcessFrames: fragment decryption failed, chunk_size={}",
                                     encrypted_chunk.size());
                            return false;
                        }

                        if (offset == 0)
                        {
                            ctx->reassembly_buf.resize(total_size);
                            ctx->reassembly_total = total_size;
                            ctx->reassembly_received = 0;
                        }

                        if (static_cast<size_t>(offset) + decrypt_result.value().size() > ctx->reassembly_total)
                        {
                            LOG_WARN("ProcessFrames: fragment overflow, offset={}, chunk_size={}, total={}",
                                     offset, decrypt_result.value().size(), ctx->reassembly_total);
                            return false;
                        }

                        std::memcpy(ctx->reassembly_buf.data() + offset, decrypt_result.value().data(),
                                    decrypt_result.value().size());
                        ctx->reassembly_received += static_cast<uint32_t>(decrypt_result.value().size());

                        if (ctx->reassembly_received == ctx->reassembly_total)
                        {
                            auto data_copy = std::make_shared<std::vector<uint8_t>>(std::move(ctx->reassembly_buf));
                            ctx->reassembly_buf.clear();
                            ctx->reassembly_total = 0;
                            ctx->reassembly_received = 0;
                            auto client_id = reinterpret_cast<ClientId>(ctx->pipe);
                            auto callback = callback_;

                            auto enqueue_result = GetGlobalThreadPool().Enqueue(
                                [callback, client_id, data_copy, this]()
                                {
                                    auto cb_send = [this](const ClientId id, const std::vector<uint8_t>& buf) -> bool
                                    {
                                        const auto result = SendToClient(id, buf);
                                        return result.has_value();
                                    };
                                    if (callback)
                                    {
                                        if (const auto optional = callback(client_id, *data_copy, cb_send); !optional)
                                        {
                                            CloseClient(client_id);
                                        }
                                    }
                                });
                            if (!enqueue_result)
                            {
                                LOG_WARN("Thread pool stopped, cannot process data for client_id={}", client_id);
                            }
                        }
                    }
                    else
                    {
                        LOG_WARN("ProcessFrames: unknown frame type={}, state=ESTABLISHED", static_cast<int>(type));
                        return false;
                    }
                }
            }
            PostRead(ctx);
            return true;
        }

        bool StartWrite(const std::shared_ptr<ClientContext>& ctx)
        {
            if (ctx->pending_writes.empty())
                return true;
            ctx->writing = true;
            const HANDLE hEvent = ctx->write_ov.hEvent;
            memset(&ctx->write_ov, 0, sizeof(ctx->write_ov));
            ctx->write_ov.hEvent = hEvent;
            DWORD bytes = 0;
            const BOOL ok = WriteFile(ctx->pipe, ctx->pending_writes.data(),
                                      static_cast<DWORD>(ctx->pending_writes.size()),
                                      &bytes, &ctx->write_ov);
            if (!ok && GetLastError() != ERROR_IO_PENDING)
            {
                ctx->writing = false;
                return false;
            }
            return true;
        }

        void CloseClient(const std::shared_ptr<ClientContext>& ctx)
        {
            LOG_INFO("CloseClient(shared_ptr), pipe={}, connected={}", reinterpret_cast<uintptr_t>(ctx->pipe),
                     ctx->connected);
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = clients_.find(reinterpret_cast<ClientId>(ctx->pipe));
            if (it == clients_.end())
                return;
            clients_.erase(it);
            CancelIoEx(ctx->pipe, &ctx->read_ov);
            CancelIoEx(ctx->pipe, &ctx->write_ov);
            DisconnectNamedPipe(ctx->pipe);
            CloseHandle(ctx->pipe);
        }

        void CloseClient(const ClientId client_id)
        {
            std::shared_ptr<ClientContext> ctx;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto it = clients_.find(client_id);
                if (it == clients_.end())
                    return;
                ctx = it->second;
                clients_.erase(it);
            }
            CancelIoEx(ctx->pipe, &ctx->read_ov);
            CancelIoEx(ctx->pipe, &ctx->write_ov);
            DisconnectNamedPipe(ctx->pipe);
            CloseHandle(ctx->pipe);
        }

        std::string server_name_;
        std::atomic<bool> running_;
        HANDLE iocp_;
        std::vector<std::thread> workers_;
        std::mutex mutex_;
        std::unordered_map<ClientId, std::shared_ptr<ClientContext>> clients_;
        ServerRecvDataCallback callback_;
    };

    std::unique_ptr<IClientConnectionImpl> CreateClientConnectionImpl()
    {
        return std::unique_ptr<IClientConnectionImpl>(new ClientConnectionImplWin());
    }

    std::unique_ptr<IServerImpl> CreateServerImpl()
    {
        return std::unique_ptr<IServerImpl>(new ServerImplWin());
    }
} // namespace tyke
#endif// _WIN32
