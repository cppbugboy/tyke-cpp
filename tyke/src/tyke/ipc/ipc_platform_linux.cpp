#ifndef _WIN32
#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#include "tyke/common/log_def.h"
#include "tyke/common/tyke_utils.h"
#include "tyke/component/thread_pool.h"
#include "tyke/ipc/ipc_frame.h"
#include "tyke/ipc/ipc_internal_platform.h"

namespace tyke
{
    class ClientConnectionImplLinux : public IClientConnectionImpl
    {
    public:
        ClientConnectionImplLinux() = default;

        ~ClientConnectionImplLinux() override
        {
            Close();
        }

        BoolResult Connect(std::string_view server_name, uint32_t, const uint32_t rw_timeout_ms) override
        {
            LOG_INFO("ipc client connecting to: {}", server_name);
            if (!utils::IsValidServerName(server_name))
                return nonstd::make_unexpected("invalid server name");
            if (server_name.size() + 5 >= sizeof(sockaddr_un::sun_path) - 1)
                return nonstd::make_unexpected("server name too long for unix domain socket");
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
            addr.sun_path[0] = '\0';
            snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, "tyke_%s", server_name.data());
            if (connect(fd_, reinterpret_cast<sockaddr*>(&addr),
                        sizeof(sa_family_t) + strlen(addr.sun_path + 1) + 1) < 0)
            {
                Close();
                return nonstd::make_unexpected(std::string("connect failed for: ") + std::string(server_name));
            }
            return true;
        }

        BoolResult Write(const void* data, const size_t size, uint32_t) override
        {
            bool expected = false;
            if (!in_use_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return nonstd::make_unexpected("connection busy: concurrent operation detected");
            struct ScopedRelease
            {
                std::atomic<bool>& flag;
                ~ScopedRelease() { flag.store(false, std::memory_order_release); }
            } guard{in_use_};
            if (size <= frame::kFragmentChunkSize)
            {
                const std::vector<uint8_t> pt(static_cast<const uint8_t*>(data),
                                              static_cast<const uint8_t*>(data) + size);
                const auto frame = frame::FrameParser::BuildFrame(frame::kMsgData, pt);
                return WriteExact(frame.data(), frame.size());
            }

            const auto* ptr = static_cast<const uint8_t*>(data);
            size_t remaining = size;
            size_t offset = 0;
            while (remaining > 0)
            {
                const size_t chunk_size = std::min(remaining, static_cast<size_t>(frame::kFragmentChunkSize));
                std::vector<uint8_t> fragment_payload;
                frame::FrameParser::EncodeLe32(static_cast<uint32_t>(size), fragment_payload);
                frame::FrameParser::EncodeLe32(static_cast<uint32_t>(offset), fragment_payload);
                std::vector<uint8_t> chunk(ptr, ptr + chunk_size);
                fragment_payload.insert(fragment_payload.end(), chunk.begin(), chunk.end());
                auto frame = frame::FrameParser::BuildFrame(frame::kMsgDataFragment, fragment_payload);
                auto write_result = WriteExact(frame.data(), frame.size());
                if (!write_result)
                    return write_result;
                ptr += chunk_size;
                offset += chunk_size;
                remaining -= chunk_size;
            }
            return true;
        }

        BoolResult ReadLoop(const ClientRecvDataCallback& callback, uint32_t timeout_ms) override
        {
            bool expected = false;
            if (!in_use_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return nonstd::make_unexpected("connection busy: concurrent operation detected");
            struct ScopedRelease
            {
                std::atomic<bool>& flag;
                ~ScopedRelease() { flag.store(false, std::memory_order_release); }
            } guard{in_use_};
            std::vector<uint8_t> raw_buf;
            std::vector<uint8_t> plain_buf;
            uint8_t chunk[131072];
            while (true)
            {
                // Use poll() with the per-call timeout before recv(), so timeout
                // behavior is consistent with the Windows WaitForSingleObject path.
                pollfd pfd{};
                pfd.fd = fd_;
                pfd.events = POLLIN;
                const int poll_ret = poll(&pfd, 1, static_cast<int>(timeout_ms));
                if (poll_ret < 0)
                {
                    if (errno == EINTR)
                        continue;
                    return nonstd::make_unexpected("poll failed with errno " + std::to_string(errno));
                }
                if (poll_ret == 0)
                    return nonstd::make_unexpected("read timeout after " + std::to_string(timeout_ms) + "ms");

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
                uint8_t type = 0;
                std::vector<uint8_t> payload;
                while (frame::FrameParser::ExtractFrame(raw_buf, type, payload))
                {
                    if (type == frame::kMsgData)
                    {
                        plain_buf.insert(plain_buf.end(), payload.begin(), payload.end());
                    }
                    else if (type == frame::kMsgDataFragment)
                    {
                        auto reassemble_result = ReassembleFragment(payload);
                        if (!reassemble_result)
                            return nonstd::make_unexpected("fragment failed: " + reassemble_result.error());
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
                    // 回调返回 false 表示继续读取，必须清空已交付的数据，
                    // 否则下一轮 callback 会收到累积的重复数据（与 Go 实现对齐）
                    plain_buf.clear();
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
            reassembly_buf_.clear();
            reassembly_total_ = 0;
            reassembly_received_ = 0;
        }

        bool IsValid() const override
        {
            return fd_ >= 0;
        }

    private:
        BoolResult WriteExact(const void* data, size_t size) const
        {
            auto ptr = static_cast<const uint8_t*>(data);
            size_t remaining = size;
            while (remaining > 0)
            {
                const ssize_t sent = send(fd_, ptr, remaining, MSG_NOSIGNAL);
                if (sent < 0)
                {
                    if (errno == EINTR)
                        continue;
                    return nonstd::make_unexpected("send failed with errno " + std::to_string(errno));
                }
                if (sent == 0)
                    return nonstd::make_unexpected("send returned 0");
                ptr += sent;
                remaining -= sent;
            }
            return true;
        }

        BoolResult ReassembleFragment(const std::vector<uint8_t>& payload)
        {
            if (payload.size() < frame::kFragmentHeaderSize)
                return nonstd::make_unexpected("fragment payload too small");
            const uint32_t total_size = frame::FrameParser::DecodeLe32(payload.data());
            const uint32_t offset = frame::FrameParser::DecodeLe32(payload.data() + 4);
            const std::vector<uint8_t> chunk(payload.begin() + frame::kFragmentHeaderSize, payload.end());
            if (total_size == 0 || total_size > frame::kMaxMessageSize)
                return nonstd::make_unexpected("fragment total_size out of range");
            if (offset > total_size)
                return nonstd::make_unexpected("fragment offset > total_size");
            if (offset == 0)
            {
                reassembly_buf_.resize(total_size);
                reassembly_total_ = total_size;
                reassembly_received_ = 0;
            }
            if (static_cast<size_t>(offset) + chunk.size() > reassembly_total_)
                return nonstd::make_unexpected("fragment offset overflow");
            std::memcpy(reassembly_buf_.data() + offset, chunk.data(), chunk.size());
            reassembly_received_ += static_cast<uint32_t>(chunk.size());
            return reassembly_received_ == reassembly_total_;
        }

        int fd_ = -1;
        std::vector<uint8_t> reassembly_buf_;
        uint32_t reassembly_total_ = 0;
        uint32_t reassembly_received_ = 0;
        // CAS 守卫：防止同一连接被多线程并发收发，破坏内部缓冲区状态。
        std::atomic<bool> in_use_{false};
    };

    constexpr int kMaxEvents = 100;

    // NOTE(N-M8): ServerImplLinux 使用单线程 epoll 模型（1 个 worker_ 线程），
    // 而 Windows ServerImplWin 使用 max(2, hardware_concurrency) 个 IOCP 工作线程。
    // 在 Linux 高并发场景下，单线程可能成为瓶颈——所有客户端 I/O 和帧处理串行化。
    // 业务逻辑通过 ThreadPool 分发，但 I/O 路径仍然串行。若需提升并发度，
    // 可改为多线程共享 epoll fd（EPOLLEXCLUSIVE / SO_REUSEPORT）。
    class ServerImplLinux : public IServerImpl, public std::enable_shared_from_this<ServerImplLinux>
    {
        struct ClientContext
        {
            int fd;
            std::vector<uint8_t> raw_recv_buf;
            std::vector<uint8_t> pending_writes;
            std::atomic<bool> writing{false};
            std::mutex write_mutex;
            std::vector<uint8_t> reassembly_buf;
            uint32_t reassembly_total = 0;
            uint32_t reassembly_received = 0;

            explicit ClientContext(int f) : fd(f), writing(false)
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
            if (server_name.size() + 5 >= sizeof(sockaddr_un::sun_path) - 1)
                return nonstd::make_unexpected("server name too long for unix domain socket");
            if (running_.load())
                return nonstd::make_unexpected("server already running");
            callback_ = std::move(callback);
            server_name_ = std::string(server_name);
            listen_fd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (listen_fd_ < 0)
                return nonstd::make_unexpected("socket creation failed");
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            addr.sun_path[0] = '\0';
            snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, "tyke_%s", server_name.data());
            if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr),
                     sizeof(sa_family_t) + strlen(addr.sun_path + 1) + 1) <
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
            epoll_fd_ = epoll_create1(0);
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
            constexpr uint64_t one = 1;
            write(wakeup_fd_, &one, sizeof(one));
            if (worker_.joinable())
                worker_.join();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& [fst, snd] : clients_)
                    close(fst);
                clients_.clear();
            }
            close(listen_fd_);
            close(epoll_fd_);
            close(wakeup_fd_);
        }

        BoolResult SendToClient(const ClientId id, const std::vector<uint8_t>& data) override
        {
            std::shared_ptr<ClientContext> ctx;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto it = clients_.find(static_cast<int>(id));
                if (it == clients_.end())
                    return nonstd::make_unexpected("client not found: " + std::to_string(id));
                ctx = it->second;
            }

            if (data.size() <= frame::kFragmentChunkSize)
            {
                auto frame = frame::FrameParser::BuildFrame(frame::kMsgData, data);
                std::lock_guard<std::mutex> lock(ctx->write_mutex);
                ctx->pending_writes.insert(ctx->pending_writes.end(), frame.begin(), frame.end());
                if (!ctx->writing)
                {
                    if (!StartWrite(ctx))
                        return nonstd::make_unexpected("SendToClient: StartWrite failed");
                }
                return true;
            }

            size_t offset = 0;
            size_t remaining = data.size();
            std::lock_guard<std::mutex> lock(ctx->write_mutex);
            while (remaining > 0)
            {
                const size_t chunk_size = std::min(remaining, static_cast<size_t>(frame::kFragmentChunkSize));
                std::vector<uint8_t> fragment_payload;
                frame::FrameParser::EncodeLe32(static_cast<uint32_t>(data.size()), fragment_payload);
                frame::FrameParser::EncodeLe32(static_cast<uint32_t>(offset), fragment_payload);
                std::vector<uint8_t> chunk(data.begin() + offset, data.begin() + offset + chunk_size);
                fragment_payload.insert(fragment_payload.end(), chunk.begin(), chunk.end());
                auto frame = frame::FrameParser::BuildFrame(frame::kMsgDataFragment, fragment_payload);
                ctx->pending_writes.insert(ctx->pending_writes.end(), frame.begin(), frame.end());
                offset += chunk_size;
                remaining -= chunk_size;
            }
            if (!ctx->writing)
            {
                if (!StartWrite(ctx))
                    return nonstd::make_unexpected("SendToClient: StartWrite failed");
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
                                const auto ctx = std::make_shared<ClientContext>(client);
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
                            uint8_t buf[131072];
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

        bool ProcessFrames(const std::shared_ptr<ClientContext>& ctx)
        {
            uint8_t type = 0;
            std::vector<uint8_t> payload;
            while (frame::FrameParser::ExtractFrame(ctx->raw_recv_buf, type, payload))
            {
                if (type == frame::kMsgData)
                {
                    auto data_copy = std::make_shared<std::vector<uint8_t>>(std::move(payload));
                    auto client_id = static_cast<ClientId>(ctx->fd);
                    auto callback = callback_;
                    auto self = shared_from_this();

                    GetGlobalThreadPool().Enqueue(
                        [callback, client_id, data_copy, self]()
                        {
                            if (!self->running_.load())
                                return;
                            auto cb_send = [self](const ClientId id, const std::vector<uint8_t>& buf) -> bool
                            {
                                const auto result = self->SendToClient(id, buf);
                                return result.has_value();
                            };
                            if (callback)
                            {
                                if (const auto optional = callback(client_id, *data_copy, cb_send); !optional)
                                {
                                    self->CloseClient(client_id);
                                }
                            }
                        });
                }
                else if (type == frame::kMsgDataFragment)
                {
                    if (payload.size() < frame::kFragmentHeaderSize)
                        return false;
                    const uint32_t total_size = frame::FrameParser::DecodeLe32(payload.data());
                    const uint32_t offset = frame::FrameParser::DecodeLe32(payload.data() + 4);
                    std::vector<uint8_t> chunk(payload.begin() + frame::kFragmentHeaderSize, payload.end());

                    if (total_size == 0 || total_size > frame::kMaxMessageSize)
                        return false;
                    if (offset > total_size)
                        return false;

                    if (offset == 0)
                    {
                        ctx->reassembly_buf.resize(total_size);
                        ctx->reassembly_total = total_size;
                        ctx->reassembly_received = 0;
                    }

                    if (static_cast<size_t>(offset) + chunk.size() > ctx->reassembly_total)
                        return false;

                    std::memcpy(ctx->reassembly_buf.data() + offset, chunk.data(), chunk.size());
                    ctx->reassembly_received += static_cast<uint32_t>(chunk.size());

                    if (ctx->reassembly_received == ctx->reassembly_total)
                    {
                        auto data_copy = std::make_shared<std::vector<uint8_t>>(std::move(ctx->reassembly_buf));
                        ctx->reassembly_buf.clear();
                        ctx->reassembly_total = 0;
                        ctx->reassembly_received = 0;
                        auto client_id = static_cast<ClientId>(ctx->fd);
                        auto callback = callback_;
                        auto self = shared_from_this();

                        GetGlobalThreadPool().Enqueue(
                            [callback, client_id, data_copy, self]()
                            {
                                if (!self->running_.load())
                                    return;
                                auto cb_send = [self](const ClientId id, const std::vector<uint8_t>& buf) -> bool
                                {
                                    const auto result = self->SendToClient(id, buf);
                                    return result.has_value();
                                };
                                if (callback)
                                {
                                    if (const auto optional = callback(client_id, *data_copy, cb_send); !optional)
                                    {
                                        self->CloseClient(client_id);
                                    }
                                }
                            });
                    }
                }
                else
                {
                    return false;
                }
            }
            return true;
        }

        bool StartWrite(const std::shared_ptr<ClientContext>& ctx) const
        {
            if (ctx->pending_writes.empty())
                return true;
            ctx->writing = true;
            ssize_t sent = send(ctx->fd, ctx->pending_writes.data(), ctx->pending_writes.size(), MSG_NOSIGNAL);
            if (sent < 0)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    ctx->writing = false;
                    return false;
                }
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

        void CloseClient(const int fd)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                clients_.erase(fd);
            }
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
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

    std::shared_ptr<IServerImpl> CreateServerImpl()
    {
        return std::shared_ptr<ServerImplLinux>(new ServerImplLinux());
    }
} // namespace tyke
#endif
