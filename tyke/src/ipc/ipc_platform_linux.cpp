/**
 * @file ipc_platform_linux.cpp
 * @brief Linux平台IPC实现。基于Unix域套接字和epoll的IPC服务端实现。
 * @author Nick
 * @date 2026/04/19
 */

#ifndef _WIN32
#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#include "common/log_def.h"
#include "common/tyke_utils.h"
#include "component/thread_pool.h"
#include "ipc/ipc_crypto.h"
#include "ipc/ipc_internal_platform.h"

namespace tyke
{
class ClientConnectionImplLinux : public IClientConnectionImpl
{
public:
    ClientConnectionImplLinux() = default;
    ~ClientConnectionImplLinux() override
    { Close(); }

    BoolResult Connect(std::string_view server_name, uint32_t, const uint32_t rw_timeout_ms) override
    {
        LOG_INFO("ipc client connecting to: {}", server_name);
        if (!utils::IsValidServerName(server_name))
            return nonstd::make_unexpected("invalid server name");
        fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0)
            return nonstd::make_unexpected("socket creation failed");
        timeval tv{};
        tv.tv_sec  = rw_timeout_ms / 1000;
        tv.tv_usec = (rw_timeout_ms % 1000) * 1000;
        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        sockaddr_un addr{};
        addr.sun_family  = AF_UNIX;
        addr.sun_path[0] = '\0';
        snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, "tyke_%s", server_name.data());
        if (connect(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(sa_family_t) + strlen(addr.sun_path + 1) + 1) < 0)
        {
            Close();
            return nonstd::make_unexpected(std::string("connect failed for: ") + std::string(server_name));
        }
        return DoHandshake();
    }

    BoolResult WriteEncrypted(const void *data, const size_t size, uint32_t) override
    {
        const std::vector<uint8_t> pt(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + size);
        auto                       encrypt_result = cipher_.Encrypt(pt);
        if (!encrypt_result)
            return nonstd::make_unexpected("encrypt failed: " + encrypt_result.error());
        const auto frame = crypto::FrameParser::BuildFrame(crypto::kMsgData, encrypt_result.value());
        return WriteExact(frame.data(), frame.size());
    }

    BoolResult ReadLoop(const ClientRecvDataCallback &callback, uint32_t) override
    {
        std::vector<uint8_t> raw_buf;
        std::vector<uint8_t> plain_buf;
        uint8_t              chunk[4096];
        while (true)
        {
            const ssize_t n = recv(fd_, chunk, sizeof(chunk), MSG_NOSIGNAL);
            if (n < 0)
            {
                if (errno == EINTR)
                    continue;
                return nonstd::make_unexpected("recv failed with errno " + std::to_string(errno));
            }
            if (n == 0)
                break;
            raw_buf.insert(raw_buf.end(), chunk, chunk + n);
            uint8_t              type = 0;
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
        if (fd_ >= 0)
        {
            close(fd_);
            fd_ = -1;
        }
    }

    bool IsValid() const override
    { return fd_ >= 0 && cipher_.IsInitialized(); }

private:
    BoolResult WriteExact(const void *data, size_t size) const
    {
        auto   ptr       = static_cast<const uint8_t *>(data);
        size_t remaining = size;
        while (remaining > 0)
        {
            const ssize_t sent = send(fd_, ptr, remaining, MSG_NOSIGNAL);
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
        if (auto gen_result = ecdh.GenerateKey(); !gen_result)
            return nonstd::make_unexpected("handshake: key generation failed: " + gen_result.error());
        auto pub_der_result = ecdh.GetPublicKeyDer();
        if (!pub_der_result)
            return nonstd::make_unexpected("handshake: get public key failed: " + pub_der_result.error());
        auto init_frame = crypto::FrameParser::BuildFrame(crypto::kMsgHandshakeInit, pub_der_result.value());
        if (auto write_result = WriteExact(init_frame.data(), init_frame.size()); !write_result)
            return nonstd::make_unexpected("handshake: write init frame failed: " + write_result.error());

        std::vector<uint8_t> raw_buf;
        uint8_t              chunk[1024];
        uint8_t              type = 0;
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

    int                  fd_ = -1;
    crypto::AesGcmCipher cipher_;
};

constexpr int kMaxEvents = 100;

class ServerImplLinux : public IServerImpl
{
    enum ClientState
    {
        STATE_WAIT_HELLO,
        STATE_ESTABLISHED
    };

    struct ClientContext
    {
        int                     fd;
        ClientState             state;
        crypto::EcdhKeyExchange ecdh;
        crypto::AesGcmCipher    cipher;
        std::vector<uint8_t>    raw_recv_buf;
        std::vector<uint8_t>    pending_writes;
        std::atomic<bool>       writing{false};
        std::mutex              write_mutex;

        explicit ClientContext(int f) : fd(f), state(STATE_WAIT_HELLO), writing(false)
        {
        }
    };

public:
    ServerImplLinux() : running_(false), listen_fd_(-1), epoll_fd_(-1), wakeup_fd_(-1)
    {
    }

    BoolResult Start(std::string_view server_name, ServerRecvDataCallback callback) override
    {
        LOG_INFO("ipc server starting on: {}", server_name);
        if (!utils::IsValidServerName(server_name))
            return nonstd::make_unexpected("invalid server name");
        if (running_.load())
            return nonstd::make_unexpected("server already running");
        callback_    = std::move(callback);
        server_name_ = std::string(server_name);
        listen_fd_   = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listen_fd_ < 0)
            return nonstd::make_unexpected("socket creation failed");
        sockaddr_un addr{};
        addr.sun_family  = AF_UNIX;
        addr.sun_path[0] = '\0';
        snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, "tyke_%s", server_name.data());
        if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(sa_family_t) + strlen(addr.sun_path + 1) + 1) <
            0)
        {
            close(listen_fd_);
            return nonstd::make_unexpected("bind failed");
        }
        if (listen(listen_fd_, SOMAXCONN) < 0)
        {
            close(listen_fd_);
            return nonstd::make_unexpected("listen failed");
        }
        epoll_fd_  = epoll_create1(0);
        wakeup_fd_ = eventfd(0, EFD_NONBLOCK);
        if (epoll_fd_ < 0 || wakeup_fd_ < 0)
        {
            close(listen_fd_);
            if (epoll_fd_ >= 0)
                close(epoll_fd_);
            if (wakeup_fd_ >= 0)
                close(wakeup_fd_);
            return nonstd::make_unexpected("epoll/eventfd creation failed");
        }
        epoll_event ev{};
        ev.events  = EPOLLIN;
        ev.data.fd = listen_fd_;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);
        ev.data.fd = wakeup_fd_;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev);

        running_ = true;
        worker_  = std::thread(&ServerImplLinux::WorkerProc, this);
        return true;
    }

    void Stop() override
    {
        LOG_INFO("ipc server stopping");
        if (!running_.exchange(false))
            return;
        constexpr uint64_t one = 1;
        write(wakeup_fd_, &one, sizeof(one));
        if (worker_.joinable())
            worker_.join();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &[fst, snd]: clients_)
                close(fst);
            clients_.clear();
        }
        close(listen_fd_);
        close(epoll_fd_);
        close(wakeup_fd_);
    }

    BoolResult SendToClient(const ClientId id, const std::vector<uint8_t> &data) override
    {
        std::shared_ptr<ClientContext> ctx;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto                  it = clients_.find(static_cast<int>(id));
            if (it == clients_.end())
                return nonstd::make_unexpected("client not found: " + std::to_string(id));
            ctx = it->second;
        }
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

private:
    void WorkerProc()
    {
        epoll_event events[kMaxEvents];
        while (running_)
        {
            const int n = epoll_wait(epoll_fd_, events, kMaxEvents, -1);
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
                        {
                            const auto                  ctx = std::make_shared<ClientContext>(client);
                            std::lock_guard<std::mutex> lock(mutex_);
                            clients_[client] = ctx;
                        }
                        epoll_event ev{};
                        ev.events  = EPOLLIN | EPOLLRDHUP;
                        ev.data.fd = client;
                        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client, &ev);
                    }
                }
                else
                {
                    std::shared_ptr<ClientContext> ctx;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto                        it = clients_.find(fd);
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
                            if (const ssize_t len = recv(fd, buf, sizeof(buf), MSG_NOSIGNAL); len > 0)
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

    bool ProcessFrames(const std::shared_ptr<ClientContext> &ctx)
    {
        uint8_t              type = 0;
        std::vector<uint8_t> payload;
        while (crypto::FrameParser::ExtractFrame(ctx->raw_recv_buf, type, payload))
        {
            if (ctx->state == STATE_WAIT_HELLO)
            {
                if (type != crypto::kMsgHandshakeInit)
                    return false;
                if (auto gen_result = ctx->ecdh.GenerateKey(); !gen_result)
                    return false;
                auto secret_result = ctx->ecdh.ComputeSharedSecret(payload);
                if (!secret_result)
                    return false;
                if (auto init_result = ctx->cipher.Init(secret_result.value()); !init_result)
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
                auto callback  = callback_;

                GetGlobalThreadPool().Enqueue(
                        [callback, client_id, data_copy, this]()
                        {
                            auto cb_send = [this](const ClientId id, const std::vector<uint8_t> &buf) -> bool
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
            }
        }
        return true;
    }

    bool StartWrite(const std::shared_ptr<ClientContext> &ctx) const
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
        ev.events    = ctx->pending_writes.empty() ? (EPOLLIN | EPOLLRDHUP) : (EPOLLIN | EPOLLOUT | EPOLLRDHUP);
        ctx->writing = !ctx->pending_writes.empty();
        ev.data.fd   = ctx->fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, ctx->fd, &ev);
        return true;
    }

    void CloseClient(const int fd)
    {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(fd);
    }

    std::string                                             server_name_;
    std::atomic<bool>                                       running_;
    int                                                     listen_fd_, epoll_fd_, wakeup_fd_;
    std::thread                                             worker_;
    std::mutex                                              mutex_;
    std::unordered_map<int, std::shared_ptr<ClientContext>> clients_;
    ServerRecvDataCallback                                  callback_;
};

std::unique_ptr<IClientConnectionImpl> CreateClientConnectionImpl()
{ return std::unique_ptr<IClientConnectionImpl>(new ClientConnectionImplLinux()); }

std::unique_ptr<IServerImpl> CreateServerImpl()
{ return std::unique_ptr<IServerImpl>(new ServerImplLinux()); }
}// namespace tyke
#endif