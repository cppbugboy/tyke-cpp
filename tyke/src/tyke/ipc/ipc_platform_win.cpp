/**
 * @file ipc_platform_win.cpp
 * @brief Windows 平台 IPC 实现。基于命名管道 (Named Pipe) + IOCP 完成端口模型的客户端和服务端。
 *
 * 客户端 (ClientConnectionImplWin)：OVERLAPPED 异步 I/O，CAS 防并发，支持大消息分片/重组。
 * 服务端 (ServerImplWin)：IOCP 工作线程池，每客户端独立读缓冲区，数据通过全局线程池异步回调。
 *
 * @author Nick
 * @date 2026/04/19
 */

#ifdef _WIN32
#include "tyke/common/log_def.h"
#include "tyke/common/tyke_utils.h"
#include "tyke/component/thread_pool.h"
#include "tyke/ipc/ipc_frame.h"
#include "tyke/ipc/ipc_internal_platform.h"
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
    // 原子计数器，为每个客户端生成唯一 ID，避免将 HANDLE 强制转换为 ClientId 的未定义行为
    static std::atomic<ClientId> g_next_client_id{1};

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

        /** @brief 连接到命名管道服务端。打开 \\.\pipe\<name>，设置 OVERLAPPED 和字节读取模式。 */
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
            // 与服务端保持一致：跳过同步 I/O 完成时的自动 IOCP 通知，
            // 避免因 Windows 默认行为导致同一操作产生多个完成包。
            SetFileCompletionNotificationModes(pipe_, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);
            DWORD mode = PIPE_READMODE_BYTE;
            SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr);
            if (rw_timeout_ms > 0)
            {
                DWORD rw_timeout = rw_timeout_ms;
                SetNamedPipeHandleState(pipe_, nullptr, &rw_timeout, nullptr);
            }
            return true;
        }

        /** @brief 通过命名管道写入数据。超过 kFragmentChunkSize 时自动分片发送。使用 CAS 防并发。 */
        BoolResult Write(const void* data, const size_t size, const uint32_t timeout_ms) override
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
                return WriteExact(frame.data(), frame.size(), timeout_ms);
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
                auto write_result = WriteExact(frame.data(), frame.size(), timeout_ms);
                if (!write_result)
                    return write_result;
                ptr += chunk_size;
                offset += chunk_size;
                remaining -= chunk_size;
            }
            return true;
        }

        /** @brief 从命名管道循环读取。持续提取帧、处理分片重组，通过回调交付完整消息。使用 CAS 防并发。 */
        BoolResult ReadLoop(const ClientRecvDataCallback& callback, const uint32_t timeout_ms) override
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
                // 持续提取帧直到数据不足（< 5 字节）。损坏帧头已被 ExtractFrame
                // 内部丢弃，若仍有足够数据则立即重试，无需等待下一次 ReadFile。
                while (raw_buf.size() >= 5)
                {
                    auto extract_result = frame::FrameParser::ExtractFrame(raw_buf, type, payload);
                    if (!extract_result)
                    {
                        if (raw_buf.size() >= 5)
                            continue;
                        break;
                    }
                    if (type == frame::kMsgData)
                    {
                        plain_buf.insert(plain_buf.end(), payload.begin(), payload.end());
                    }
                    else if (type == frame::kMsgDataFragment)
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
                    // 回调返回 false 表示继续读取，必须清空已交付的数据，
                    // 否则下一轮 callback 会收到累积的重复数据（与 Go 实现对齐）
                    plain_buf.clear();
                }
            }
            return nonstd::make_unexpected("read loop: connection closed");
        }

        /** @brief 关闭客户端连接：取消 pending I/O、关闭管道和事件句柄、清空重组缓冲区。 */
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
            return pipe_ != INVALID_HANDLE_VALUE && event_ != nullptr;
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

        HANDLE pipe_ = INVALID_HANDLE_VALUE;
        HANDLE event_ = nullptr;
        std::vector<uint8_t> reassembly_buf_;
        uint32_t reassembly_total_ = 0;
        uint32_t reassembly_received_ = 0;
        // CAS 守卫：防止同一连接被多线程并发收发，破坏 OVERLAPPED 状态。
        std::atomic<bool> in_use_{false};
    };

    class ServerImplWin : public IServerImpl, public std::enable_shared_from_this<ServerImplWin>
    {
        struct ClientContext
        {
            OVERLAPPED read_ov{};
            OVERLAPPED write_ov{};
            HANDLE pipe;
            ClientId client_id = 0;
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
                : pipe(INVALID_HANDLE_VALUE), client_id(0), connected(false), writing(false), raw_read_buf{}
            {
                // 服务端 I/O 全部走 IOCP 完成通知，OVERLAPPED.hEvent 不需要。
                // 旧实现在这里创建手动重置事件，但既不用于 WaitForSingleObject 等待，
                // 又因 ConnectNamedPipe 完成后被置为信号态且从不重置，反而干扰 ReadFile。
                memset(&read_ov, 0, sizeof(read_ov));
                memset(&write_ov, 0, sizeof(write_ov));
            }

            ~ClientContext() = default;
        };

    public:
        ServerImplWin() : running_(false), iocp_(nullptr)
        {
        }

        /** @brief 启动 IOCP 服务端：创建完成端口、预创建命名管道实例、启动工作线程池。 */
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
            while (running_)
            {
                DWORD bytes = 0;
                ULONG_PTR key = 0;
                OVERLAPPED* ov = nullptr;
                // 使用有限超时（100 ms），避免 Stop() 与完成处理竞态时工作线程无限阻塞。
                // 每次迭代重新检查 running_，保证及时关闭。
                const BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &ov, 100);
                if (!running_)
                    break;
                const auto client_id = static_cast<ClientId>(key);
                if (client_id == 0)
                    continue;

                std::shared_ptr<ClientContext> ctx;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = clients_.find(client_id);
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
                    LOG_INFO("Client connected, client_id={}", ctx->client_id);
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
                            LOG_WARN("ProcessFrames failed, closing client, client_id={}",
                                     ctx->client_id);
                            CloseClient(ctx);
                            continue;
                        }
                    }
                    else
                    {
                        LOG_INFO("Read completion with 0 bytes (client disconnected), client_id={}",
                                 ctx->client_id);
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
            ctx->client_id = g_next_client_id.fetch_add(1, std::memory_order_relaxed);
            if (!CreateIoCompletionPort(pipe, iocp_, static_cast<ULONG_PTR>(ctx->client_id), 0))
            {
                CloseHandle(pipe);
                return nonstd::make_unexpected("CreateIoCompletionPort for pipe failed");
            }
            // 禁止同步 ReadFile 成功时 Windows 自动投递 IOCP 完成包。
            // 所有完成通知均由代码中的 PostQueuedCompletionStatus 显式投递，
            // 避免同一操作产生双重/多重完成包导致 DataCallback 被重复调用。
            SetFileCompletionNotificationModes(pipe, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                clients_[ctx->client_id] = ctx;
            }
            if (const BOOL pending = ConnectNamedPipe(pipe, &ctx->read_ov); !pending)
            {
                if (const DWORD err = GetLastError(); err == ERROR_PIPE_CONNECTED)
                {
                    PostQueuedCompletionStatus(iocp_, 0, static_cast<ULONG_PTR>(ctx->client_id), &ctx->read_ov);
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
            // 服务端走 IOCP 完成通知，OVERLAPPED.hEvent 不需要（保持 NULL）。
            memset(&ctx->read_ov, 0, sizeof(ctx->read_ov));
            DWORD bytes = 0;
            const BOOL ok = ReadFile(ctx->pipe, ctx->raw_read_buf, sizeof(ctx->raw_read_buf), &bytes,
                                     &ctx->read_ov);
            if (ok)
            {
                // 同步完成：IOCP 默认不为同步完成投递完成包，必须手动投递，
                // 否则 worker 永远收不到这次读取的完成通知（这是 6 个 IPC 集成测试
                // 失败的根因——客户端 Write 后数据已在管道缓冲区，ReadFile 同步返回 TRUE）。
                PostQueuedCompletionStatus(iocp_, bytes,
                                           static_cast<ULONG_PTR>(ctx->client_id), &ctx->read_ov);
                return;
            }
            const DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING || err == ERROR_MORE_DATA)
                return;
            if (err == ERROR_OPERATION_ABORTED || err == ERROR_INVALID_HANDLE)
                return;
            LOG_WARN("PostRead ReadFile failed, client_id={}, err={}", ctx->client_id, err);
            CloseClient(ctx);
        }

        bool ProcessFrames(const std::shared_ptr<ClientContext>& ctx)
        {
            uint8_t type = 0;
            std::vector<uint8_t> payload;
            // 持续尝试提取帧，直到缓冲区不足以容纳最小帧头（5 字节）。
            // 当 ExtractFrame 检测到损坏帧头时，它已丢弃无效字节并返回错误；
            // 此时若缓冲区仍有 ≥5 字节数据，立即重试，无需等待下一次 ReadFile。
            while (ctx->raw_recv_buf.size() >= 5)
            {
                auto extract_result = frame::FrameParser::ExtractFrame(ctx->raw_recv_buf, type, payload);
                if (!extract_result)
                {
                    // ExtractFrame 内部已丢弃无效字节。
                    // 若缓冲区仍有足够数据则立即重试；否则退出并等待 PostRead 补充数据。
                    if (ctx->raw_recv_buf.size() >= 5)
                        continue;
                    break;
                }
                if (type == frame::kMsgData)
                {
                    const auto captured_client_id = ctx->client_id;
                    auto data_copy = std::make_shared<std::vector<uint8_t>>(std::move(payload));
                    auto callback = callback_;
                    auto self = shared_from_this();

                    auto enqueue_result = GetGlobalThreadPool().Enqueue(
                        [callback, captured_client_id, data_copy, self]()
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
                                // 文档语义：回调返回 nullopt 表示"数据不完整，需等待更多数据"，
                                // 不应关闭连接。返回值（消费字节数）当前实现不消费，留待未来扩展。
                                const auto optional = callback(captured_client_id, *data_copy, cb_send);
                                (void)optional;
                            }
                        });
                    if (!enqueue_result)
                    {
                        LOG_WARN("Thread pool stopped, cannot process data for client_id={}", captured_client_id);
                    }
                }
                else if (type == frame::kMsgDataFragment)
                {
                    if (payload.size() < frame::kFragmentHeaderSize)
                    {
                        LOG_WARN("ProcessFrames: fragment payload too small, size={}", payload.size());
                        return false;
                    }
                    const uint32_t total_size = frame::FrameParser::DecodeLe32(payload.data());
                    const uint32_t offset = frame::FrameParser::DecodeLe32(payload.data() + 4);
                    std::vector<uint8_t> chunk(payload.begin() + frame::kFragmentHeaderSize, payload.end());

                    if (total_size == 0 || total_size > frame::kMaxMessageSize)
                    {
                        LOG_WARN("ProcessFrames: fragment total_size out of range, total_size={}", total_size);
                        return false;
                    }
                    if (offset > total_size)
                    {
                        LOG_WARN("ProcessFrames: fragment offset > total_size, offset={}, total_size={}",
                                 offset, total_size);
                        return false;
                    }

                    if (offset == 0)
                    {
                        ctx->reassembly_buf.resize(total_size);
                        ctx->reassembly_total = total_size;
                        ctx->reassembly_received = 0;
                    }

                    if (static_cast<size_t>(offset) + chunk.size() > ctx->reassembly_total)
                    {
                        LOG_WARN("ProcessFrames: fragment overflow, offset={}, chunk_size={}, total={}",
                                 offset, chunk.size(), ctx->reassembly_total);
                        return false;
                    }

                    std::memcpy(ctx->reassembly_buf.data() + offset, chunk.data(), chunk.size());
                    ctx->reassembly_received += static_cast<uint32_t>(chunk.size());

                    if (ctx->reassembly_received == ctx->reassembly_total)
                    {
                        const auto captured_client_id = ctx->client_id;
                        auto data_copy = std::make_shared<std::vector<uint8_t>>(std::move(ctx->reassembly_buf));
                        ctx->reassembly_buf.clear();
                        ctx->reassembly_total = 0;
                        ctx->reassembly_received = 0;
                        auto callback = callback_;
                        auto self = shared_from_this();

                        auto enqueue_result = GetGlobalThreadPool().Enqueue(
                            [callback, captured_client_id, data_copy, self]()
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
                                    // 同上：nullopt 表示数据不完整，不关闭连接。
                                    const auto optional = callback(captured_client_id, *data_copy, cb_send);
                                    (void)optional;
                                }
                            });
                        if (!enqueue_result)
                        {
                            LOG_WARN("Thread pool stopped, cannot process data for client_id={}", captured_client_id);
                        }
                    }
                }
                else
                {
                    LOG_WARN("ProcessFrames: unknown frame type={}", static_cast<int>(type));
                    return false;
                }
            }
            PostRead(ctx);
            return true;
        }

        bool StartWrite(const std::shared_ptr<ClientContext>& ctx)
        {
            if (ctx->pending_writes.empty())
                return true;
            if (ctx->pipe == INVALID_HANDLE_VALUE)
            {
                ctx->writing = false;
                return false;
            }
            ctx->writing = true;
            memset(&ctx->write_ov, 0, sizeof(ctx->write_ov));
            DWORD bytes = 0;
            const BOOL ok = WriteFile(ctx->pipe, ctx->pending_writes.data(),
                                      static_cast<DWORD>(ctx->pending_writes.size()),
                                      &bytes, &ctx->write_ov);
            if (ok)
            {
                // 同步完成：手动投递完成包，让 worker 走正常的写完成流程
                // （清空 pending_writes、置 writing=false、可能继续 StartWrite）。
                PostQueuedCompletionStatus(iocp_, bytes,
                                           static_cast<ULONG_PTR>(ctx->client_id), &ctx->write_ov);
                return true;
            }
            if (GetLastError() != ERROR_IO_PENDING)
            {
                ctx->writing = false;
                return false;
            }
            return true;
        }

        void CloseClient(const std::shared_ptr<ClientContext>& ctx)
        {
            LOG_INFO("CloseClient(shared_ptr), client_id={}, connected={}", ctx->client_id,
                     ctx->connected);
            {
                std::lock_guard<std::mutex> write_lock(ctx->write_mutex);
                ctx->writing = false;
                ctx->pending_writes.clear();
            }
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = clients_.find(ctx->client_id);
            if (it == clients_.end())
                return;
            clients_.erase(it);
            CancelIoEx(ctx->pipe, &ctx->read_ov);
            CancelIoEx(ctx->pipe, &ctx->write_ov);
            DisconnectNamedPipe(ctx->pipe);
            CloseHandle(ctx->pipe);
            ctx->pipe = INVALID_HANDLE_VALUE;
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
            {
                std::lock_guard<std::mutex> write_lock(ctx->write_mutex);
                ctx->writing = false;
                ctx->pending_writes.clear();
            }
            CancelIoEx(ctx->pipe, &ctx->read_ov);
            CancelIoEx(ctx->pipe, &ctx->write_ov);
            DisconnectNamedPipe(ctx->pipe);
            CloseHandle(ctx->pipe);
            ctx->pipe = INVALID_HANDLE_VALUE;
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

    std::shared_ptr<IServerImpl> CreateServerImpl()
    {
        return std::shared_ptr<ServerImplWin>(new ServerImplWin());
    }
} // namespace tyke
#endif// _WIN32
