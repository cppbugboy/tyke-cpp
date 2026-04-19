/**
 * @file ipc_platform_linux.cpp
 * @brief Linux平台IPC实现。基于Unix域套接字和epoll的IPC服务端实现。
 * @author Nick
 * @date 2026/04/19
 */

#ifndef _WIN32
#include "ipc/ipc_internal_platform.h"
#include "ipc/ipc_crypto.h"
#include "common/log_def.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <atomic>

#include "component/thread_pool.h"

namespace tyke
{
    class ClientConnectionImplLinux : public IClientConnectionImpl
    {
    public:
        ClientConnectionImplLinux() = default;
        ~ClientConnectionImplLinux() override { Close(); }

        BoolResult Connect(const std::string& server_name, uint32_t, uint32_t rw_timeout_ms) override
        {
            LOG_INFO("ipc client connecting to: {}", server_name);
            fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd_ < 0)
                return nonstd::make_unexpected("socket creation failed");
            timeval tv{};
            tv.tv_sec = rw_timeout_ms / 1000;
            tv.tv_usec = (rw_timeout_ms % 1000) * 1000;
            setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/%s", server_name.c_str());
            if (connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            {
                Close();
                return nonstd::make_unexpected("connect failed for: " + server_name);
            }
            return DoHandshake();
        }

        BoolResult WriteEncrypted(const void* data, size_t size, uint32_t) override
        {
            const std::vector<uint8_t> pt(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
            auto encrypt_result = cipher_.Encrypt(pt);
            if (!encrypt_result)
                return nonstd::make_unexpected("encrypt failed: " + encrypt_result.error());
            auto frame = crypto::FrameParser::BuildFrame(crypto::kMsgData, encrypt_result.value());
            return WriteExact(frame.data(), frame.size());
        }

        BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t) override
        {
            std::vector<uint8_t> raw_buf;
            std::vector<uint8_t> plain_buf;
            uint8_t chunk[4096];
            while (true)
            {
                ssize_t n = recv(fd_, chunk, sizeof(chunk), MSG_NOSIGNAL);
                if (n < 0)
                {
                    if (errno == EINTR)
                        continue;
                    return nonstd::make_unexpected("recv failed with errno " + std::to_string(errno));
                }
                if (n == 0)
                    break;
                raw_buf.insert(raw_buf.end(), chunk, chunk + n);
                uint8_t type = 0;
                std::vector<uint8_t> payload;
                while (crypto::FrameParser::ExtractFrame(raw_buf, type, payload))
                {
                    if (type == crypto::kMsgData)
                    {
                        auto decrypt_result = cipher_.Decrypt(payload);
                        if (!decrypt_result)
                            return nonstd::make_unexpected("decrypt failed: " + decrypt_result.error());
                        plain_buf.insert(plain_buf.end(), decrypt_result.value().begin(), decrypt_result.value().end());
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
            if (fd_ >= 0) { close(fd_); fd_ = -1; }
        }

        bool IsValid() const override { return fd_ >= 0 && cipher_.IsInitialized(); }

    private:
        BoolResult WriteExact(const void* data, size_t size)
        {
            auto ptr = static_cast<const uint8_t*>(data);
            size_t remaining = size;
            while (remaining > 0)
            {
                ssize_t sent = send(fd_, ptr, remaining, MSG_NOSIGNAL);
                if (sent <= 0)
                    return nonstd::make_unexpected("send failed with errno " + std::to_string(errno));
                ptr += sent;
                remaining -= sent;
            }
            return true;
        }

        BoolResult DoHandshake()
        {
            crypto::EcdhKeyExchange ecdh;
            auto gen_result = ecdh.GenerateKey();
            if (!gen_result)
                return nonstd::make_unexpected("handshake: key generation failed: " + gen_result.error());
            auto pub_der_result = ecdh.GetPublicKeyDer();
            if (!pub_der_result)
                return nonstd::make_unexpected("handshake: get public key failed: " + pub_der_result.error());
            auto init_frame = crypto::FrameParser::BuildFrame(crypto::kMsgHandshakeInit, pub_der_result.value());
            auto write_result = WriteExact(init_frame.data(), init_frame.size());
            if (!write_result)
                return nonstd::make_unexpected("handshake: write init frame failed: " + write_result.error());

            std::vector<uint8_t> raw_buf;
            uint8_t chunk[1024];
            uint8_t type = 0;
            std::vector<uint8_t> payload;
            while (true)
            {
                ssize_t n = recv(fd_, chunk, sizeof(chunk), MSG_NOSIGNAL);
                if (n <= 0)
                {
                    if (errno == EINTR)
                        continue;
                    return nonstd::make_unexpected("handshake: recv failed");
                }
                raw_buf.insert(raw_buf.end(), chunk, chunk + n);
                auto extract_result = crypto::FrameParser::ExtractFrame(raw_buf, type, payload);
                if (extract_result)
                {
                    if (type == crypto::kMsgHandshakeResp)
                    {
                        auto secret_result = ecdh.ComputeSharedSecret(payload);
                        if (!secret_result)
                            return nonstd::make_unexpected("handshake: compute shared secret failed: " + secret_result.error());
                        auto init_result = cipher_.Init(secret_result.value());
                        if (!init_result)
                            return nonstd::make_unexpected("handshake: cipher init failed: " + init_result.error());
                        return true;
                    }
                    return nonstd::make_unexpected("handshake: unexpected frame type");
                }
            }
        }

        int fd_ = -1;
        crypto::AesGcmCipher cipher_;
    };

    constexpr int kMaxEvents = 100;

    class ServerImplLinux : public IServerImpl
    {
        enum ClientState { STATE_WAIT_HELLO, STATE_ESTABLISHED };

        struct ClientContext
        {
            int fd;
            ClientState state;
            crypto::EcdhKeyExchange ecdh;
            crypto::AesGcmCipher cipher;
            std::vector<uint8_t> raw_recv_buf;
            std::vector<uint8_t> pending_writes;
            bool writing;
            std::mutex write_mutex;

            explicit ClientContext(int f) : fd(f), state(STATE_WAIT_HELLO), writing(false) {}
        };

    public:
        ServerImplLinux() : running_(false), listen_fd_(-1), epoll_fd_(-1), wakeup_fd_(-1) {}

        BoolResult Start(const std::string& server_name, ServerRecvDataCallback callback) override
        {
            LOG_INFO("ipc server starting on: {}", server_name);
            if (running_.load())
                return nonstd::make_unexpected("server already running");
            callback_ = std::move(callback);
            server_name_ = "/tmp/" + server_name;
            listen_fd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (listen_fd_ < 0)
                return nonstd::make_unexpected("socket creation failed");
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", server_name_.c_str());
            unlink(addr.sun_path);
            if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            {
                close(listen_fd_);
                return nonstd::make_unexpected("bind failed");
            }
            if (listen(listen_fd_, SOMAXCONN) < 0)
            {
                close(listen_fd_);
                return nonstd::make_unexpected("listen failed");
            }
            epoll_fd_ = epoll_create1(0);
            wakeup_fd_ = eventfd(0, EFD_NONBLOCK);
            if (epoll_fd_ < 0 || wakeup_fd_ < 0)
            {
                close(listen_fd_);
                if (epoll_fd_ >= 0) close(epoll_fd_);
                if (wakeup_fd_ >= 0) close(wakeup_fd_);
                return nonstd::make_unexpected("epoll/eventfd creation failed");
            }
            epoll_event ev{};
            ev.events = EPOLLIN;
            ev.data.fd = listen_fd_;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);
            ev.data.fd = wakeup_fd_;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev);

            running_ = true;
            worker_ = std::thread(&ServerImplLinux::WorkerProc, this);
            return true;
        }

        void Stop() override
        {
            LOG_INFO("ipc server stopping");
            if (!running_.exchange(false))
                return;
            uint64_t one = 1;
            write(wakeup_fd_, &one, sizeof(one));
            if (worker_.joinable())
                worker_.join();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& kv : clients_)
                    close(kv.first);
                clients_.clear();
            }
            close(listen_fd_);
            close(epoll_fd_);
            close(wakeup_fd_);
            unlink(server_name_.c_str());
        }

        BoolResult SendToClient(ClientId id, const std::vector<uint8_t>& data) override
        {
            std::shared_ptr<ClientContext> ctx;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = clients_.find(static_cast<int>(id));
                if (it == clients_.end())
                    return nonstd::make_unexpected("client not found: " + std::to_string(id));
                ctx = it->second;
            }
            auto encrypt_result = ctx->cipher.Encrypt(data);
            if (!encrypt_result)
                return nonstd::make_unexpected("encrypt failed for client " + std::to_string(id) + ": " + encrypt_result.error());
            auto frame = crypto::FrameParser::BuildFrame(crypto::kMsgData, encrypt_result.value());

            std::lock_guard<std::mutex> lock(ctx->write_mutex);
            ctx->pending_writes.insert(ctx->pending_writes.end(), frame.begin(), frame.end());
            if (!ctx->writing)
            {
                StartWrite(ctx);
            }
            return true;
        }

    private:
        void WorkerProc()
        {
            epoll_event events[kMaxEvents];
            while (running_)
            {
                int n = epoll_wait(epoll_fd_, events, kMaxEvents, -1);
                if (n < 0)
                {
                    if (errno == EINTR)
                        continue;
                    break;
                }
                for (int i = 0; i < n; ++i)
                {
                    int fd = events[i].data.fd;
                    if (fd == wakeup_fd_)
                    {
                        uint64_t dummy;
                        read(wakeup_fd_, &dummy, sizeof(dummy));
                        continue;
                    }
                    if (fd == listen_fd_)
                    {
                        while (true)
                        {
                            int client = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK);
                            if (client < 0)
                                break;
                            auto ctx = std::make_shared<ClientContext>(client);
                            {
                                std::lock_guard<std::mutex> lock(mutex_);
                                clients_[client] = ctx;
                            }
                            epoll_event ev{};
                            ev.events = EPOLLIN | EPOLLRDHUP;
                            ev.data.fd = client;
                            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client, &ev);
                        }
                    }
                    else
                    {
                        std::shared_ptr<ClientContext> ctx;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            auto it = clients_.find(fd);
                            if (it == clients_.end())
                                continue;
                            ctx = it->second;
                        }
                        if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
                        {
                            CloseClient(fd);
                            continue;
                        }
                        if (events[i].events & EPOLLIN)
                        {
                            uint8_t buf[4096];
                            while (true)
                            {
                                ssize_t len = recv(fd, buf, sizeof(buf), MSG_NOSIGNAL);
                                if (len > 0)
                                {
                                    ctx->raw_recv_buf.insert(ctx->raw_recv_buf.end(), buf, buf + len);
                                }
                                else if (len == 0)
                                {
                                    CloseClient(fd);
                                    break;
                                }
                                else
                                {
                                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                                    {
                                        if (!ProcessFrames(ctx))
                                            CloseClient(fd);
                                    }
                                    else
                                        CloseClient(fd);
                                    break;
                                }
                            }
                        }
                        if (events[i].events & EPOLLOUT)
                        {
                            std::lock_guard<std::mutex> lock(ctx->write_mutex);
                            ctx->writing = false;
                            if (!ctx->pending_writes.empty())
                                StartWrite(ctx);
                        }
                    }
                }
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
                        return false;
                    auto gen_result = ctx->ecdh.GenerateKey();
                    if (!gen_result)
                        return false;
                    auto secret_result = ctx->ecdh.ComputeSharedSecret(payload);
                    if (!secret_result)
                        return false;
                    auto init_result = ctx->cipher.Init(secret_result.value());
                    if (!init_result)
                        return false;
                    auto pub_der_result = ctx->ecdh.GetPublicKeyDer();
                    if (!pub_der_result)
                        return false;
                    auto resp = crypto::FrameParser::BuildFrame(crypto::kMsgHandshakeResp, pub_der_result.value());

                    std::lock_guard<std::mutex> lock(ctx->write_mutex);
                    ctx->pending_writes.insert(ctx->pending_writes.end(), resp.begin(), resp.end());
                    if (!ctx->writing)
                        StartWrite(ctx);
                    ctx->state = STATE_ESTABLISHED;
                }
                else if (ctx->state == STATE_ESTABLISHED)
                {
                    if (type != crypto::kMsgData)
                        return false;
                    auto decrypt_result = ctx->cipher.Decrypt(payload);
                    if (!decrypt_result)
                        return false;
                    
                    auto data_copy = std::make_shared<std::vector<uint8_t>>(std::move(decrypt_result.value()));
                    auto client_id = static_cast<ClientId>(ctx->fd);
                    auto callback = callback_;
                    
                    THREAD_POOL_INSTANCE->Enqueue([callback, client_id, data_copy, this]() {
                        auto cb_send = [this](ClientId id, const std::vector<uint8_t>& buf) -> bool
                        {
                            auto result = SendToClient(id, buf);
                            return result.has_value();
                        };
                        if (callback)
                        {
                            const auto optional = callback(client_id, *data_copy, cb_send);
                            if (!optional)
                            {
                                CloseClient(client_id);
                            }
                        }
                    });
                }
            }
            return true;
        }

        bool StartWrite(const std::shared_ptr<ClientContext>& ctx)
        {
            if (ctx->pending_writes.empty())
                return true;
            ctx->writing = true;
            ssize_t sent = send(ctx->fd, ctx->pending_writes.data(), ctx->pending_writes.size(), MSG_NOSIGNAL);
            if (sent < 0)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    return false;
                sent = 0;
            }
            ctx->pending_writes.erase(ctx->pending_writes.begin(), ctx->pending_writes.begin() + sent);

            epoll_event ev{};
            ev.events = ctx->pending_writes.empty() ? (EPOLLIN | EPOLLRDHUP) : (EPOLLIN | EPOLLOUT | EPOLLRDHUP);
            ctx->writing = !ctx->pending_writes.empty();
            ev.data.fd = ctx->fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, ctx->fd, &ev);
            return true;
        }

        void CloseClient(int fd)
        {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            std::lock_guard<std::mutex> lock(mutex_);
            clients_.erase(fd);
        }

        std::string server_name_;
        std::atomic<bool> running_;
        int listen_fd_, epoll_fd_, wakeup_fd_;
        std::thread worker_;
        std::mutex mutex_;
        std::unordered_map<int, std::shared_ptr<ClientContext>> clients_;
        ServerRecvDataCallback callback_;
    };

    std::unique_ptr<IClientConnectionImpl> CreateClientConnectionImpl()
    {
        return std::unique_ptr<IClientConnectionImpl>(new ClientConnectionImplLinux());
    }

    std::unique_ptr<IServerImpl> CreateServerImpl()
    {
        return std::unique_ptr<IServerImpl>(new ServerImplLinux());
    }
}
#endif
