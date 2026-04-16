// clang-format off
// WinSock2.h must be included before Windows.h to avoid winsock.h type redefinitions.
#include <WinSock2.h>
// clang-format on
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#include <bee/async/async.h>
#include <bee/async/async_win.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/utility/dynarray.h>

#include <cstring>
#include <memory>
#include <utility>

// RtlNtStatusToDosError is in ntdll but not declared in standard MinGW headers.
extern "C" ULONG WINAPI RtlNtStatusToDosError(NTSTATUS Status);

// AcceptEx and ConnectEx are Winsock extension functions loaded dynamically via
// WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER).  We declare only the GUIDs and
// function-pointer typedefs we need so that MSWSock.h is not required.
typedef BOOL(PASCAL* LPFN_ACCEPTEX)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL(PASCAL* LPFN_CONNECTEX)(SOCKET, const sockaddr*, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
static const GUID k_WSAID_ACCEPTEX  = { 0xb5367df1, 0xcbac, 0x11cf, { 0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92 } };
static const GUID k_WSAID_CONNECTEX = { 0x25a207b9, 0xddf3, 0x4660, { 0x8e, 0xe9, 0x76, 0xe5, 0x8c, 0x74, 0x06, 0x3e } };


namespace bee::async {

    // sizeof(OVERLAPPED) is 32 on both x86 and x64.
    static_assert(sizeof(OVERLAPPED) <= sizeof(async::overlapped_ext::overlapped), "overlapped_ext::overlapped buffer is too small");

    static inline OVERLAPPED* as_ov(async::overlapped_ext* ext) {
        return reinterpret_cast<OVERLAPPED*>(ext->overlapped);
    }

    async::async()
        : m_iocp(nullptr)
        , m_connectex(nullptr)
        , m_acceptex(nullptr) {
        m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

        // Load ConnectEx and AcceptEx at startup using a temporary socket.
        SOCKET tmp = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (tmp != INVALID_SOCKET) {
            DWORD bytes         = 0;
GUID guid_connectex = WSAID_CONNECTEX;
            WSAIoctl(tmp, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_connectex, sizeof(guid_connectex), &m_connectex, sizeof(m_connectex), &bytes, nullptr, nullptr);
GUID guid_acceptex = WSAID_ACCEPTEX;
            WSAIoctl(tmp, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_acceptex, sizeof(guid_acceptex), &m_acceptex, sizeof(m_acceptex), &bytes, nullptr, nullptr);
            closesocket(tmp);
        }
    }

    async::~async() {
        stop();
    }

    void async::cancel(net::fd_t fd) {
        CancelIoEx(reinterpret_cast<HANDLE>(fd), nullptr);
    }

    bool async::associate(net::fd_t fd) {
        auto* h = reinterpret_cast<HANDLE>(fd);
        if (CreateIoCompletionPort(h, static_cast<HANDLE>(m_iocp), 0, 0) != m_iocp) {
            // ERROR_INVALID_PARAMETER means the handle is already associated with
            // this same IOCP (e.g. the listen socket used across multiple accepts).
            // Any other error is a real failure.
            if (GetLastError() != ERROR_INVALID_PARAMETER) {
                return false;
            }
            // Already associated: notification modes already set, nothing more to do.
            return true;
        }
        // Newly associated: skip IOCP notification on synchronous completion and
        // skip setting the handle event — all completions come through the IOCP.
        SetFileCompletionNotificationModes(h, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE);
        return true;
    }

    // Re-opens a file handle with FILE_FLAG_OVERLAPPED and associates it with
    // the IOCP. Returns {overlapped file_handle, writable} on success, or
    // {invalid file_handle, false} on failure.
    std::pair<file_handle, bool> async::associate_file(file_handle::value_type fd) {
        auto* h       = static_cast<HANDLE>(fd);
        bool writable = true;
        HANDLE ov_h   = ReOpenFile(h, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_OVERLAPPED);
        if (ov_h == INVALID_HANDLE_VALUE) {
            writable = false;
            ov_h     = ReOpenFile(h, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_OVERLAPPED);
        }
        if (ov_h == INVALID_HANDLE_VALUE) return { file_handle {}, false };
        if (CreateIoCompletionPort(ov_h, static_cast<HANDLE>(m_iocp), 0, 0) != m_iocp) {
            if (GetLastError() != ERROR_INVALID_PARAMETER) {
                CloseHandle(ov_h);
                return { file_handle {}, false };
            }
        }
        return { file_handle::from_native(ov_h), writable };
    }

    // Handle a synchronous completion (FILE_SKIP_COMPLETION_PORT_ON_SUCCESS):
    // build an io_completion from the overlapped_ext and either store it in
    // completions[] or append to m_sync_completions for later delivery.
    static io_completion make_sync_completion(std::unique_ptr<async::overlapped_ext> ext, DWORD bytes) {
        io_completion c;
        c.request_id                       = ext->request_id;
        c.bytes_transferred                = static_cast<size_t>(bytes);
        c.error_code                       = 0;
        static constexpr async_op op_map[] = {
            async_op::read,
            async_op::write,
            async_op::accept,
            async_op::connect,
            async_op::file_read,
            async_op::file_write,
            async_op::fd_poll,
        };
        c.op = op_map[static_cast<int>(ext->type)];
        if (bytes == 0 &&
            ext->type != async::overlapped_ext::op_connect &&
            ext->type != async::overlapped_ext::op_file_read &&
            ext->type != async::overlapped_ext::op_file_write &&
            ext->type != async::overlapped_ext::op_poll) {
            c.status = async_status::close;
        } else {
            c.status = async_status::success;
        }
        return c;
    }

    bool async::submit_read(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) {
        auto ext = std::make_unique<overlapped_ext>();
        memset(ext->overlapped, 0, sizeof(ext->overlapped));
        ext->request_id  = request_id;
        ext->type        = overlapped_ext::op_read;
        ext->accept_sock = 0;

        static_assert(sizeof(net::socket::iobuf) == sizeof(WSABUF));
        DWORD flags = 0;
        DWORD bytes = 0;
        int rc      = WSARecv(static_cast<SOCKET>(fd), reinterpret_cast<WSABUF*>(const_cast<net::socket::iobuf*>(bufs.data())), static_cast<DWORD>(bufs.size()), &bytes, &flags, reinterpret_cast<LPWSAOVERLAPPED>(as_ov(ext.get())), nullptr);
        if (rc == 0) {
            m_sync_completions.push_back(make_sync_completion(std::move(ext), bytes));
            return true;
        }
        if (WSAGetLastError() != WSA_IO_PENDING) {
            return false;
        }
        ext.release();  // ownership transferred to IOCP
        return true;
    }

    bool async::submit_write(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) {
        auto ext = std::make_unique<overlapped_ext>();
        memset(ext->overlapped, 0, sizeof(ext->overlapped));
        ext->request_id  = request_id;
        ext->type        = overlapped_ext::op_write;
        ext->accept_sock = 0;

        static_assert(sizeof(net::socket::iobuf) == sizeof(WSABUF));
        DWORD bytes = 0;
        int rc      = WSASend(static_cast<SOCKET>(fd), reinterpret_cast<WSABUF*>(const_cast<net::socket::iobuf*>(bufs.data())), static_cast<DWORD>(bufs.size()), &bytes, 0, reinterpret_cast<LPWSAOVERLAPPED>(as_ov(ext.get())), nullptr);
        if (rc == 0) {
            m_sync_completions.push_back(make_sync_completion(std::move(ext), bytes));
            return true;
        }
        if (WSAGetLastError() != WSA_IO_PENDING) {
            return false;
        }
        ext.release();  // ownership transferred to IOCP
        return true;
    }

    bool async::submit_accept(net::fd_t listen_fd, uint64_t request_id) {
        // Query the address family of the listen socket so the accept socket matches.
        net::endpoint ep;
        int af = AF_INET;
        if (net::socket::getsockname(listen_fd, ep)) {
            if (ep.get_family() == net::family::inet6) {
                af = AF_INET6;
            }
        }
        SOCKET accept_sock = WSASocketW(af, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (accept_sock == INVALID_SOCKET) {
            return false;
        }
        // Associate the newly-created accept socket with the IOCP.
        if (!associate(static_cast<net::fd_t>(accept_sock))) {
            closesocket(accept_sock);
            return false;
        }

        auto ext = std::make_unique<overlapped_ext>();
        memset(ext->overlapped, 0, sizeof(ext->overlapped));
        memset(ext->addr_buf, 0, sizeof(ext->addr_buf));
        ext->request_id  = request_id;
        ext->type        = overlapped_ext::op_accept;
        ext->accept_sock = static_cast<uintptr_t>(accept_sock);
        ext->listen_sock = static_cast<uintptr_t>(static_cast<SOCKET>(listen_fd));

        if (!m_acceptex) {
            closesocket(accept_sock);
            return false;
        }
        // AcceptEx output buffer: local addr slot + remote addr slot.
        // dwReceiveDataLength = 0 (no data prefix).
        auto fn_acceptex     = reinterpret_cast<LPFN_ACCEPTEX>(m_acceptex);
        DWORD bytes_received = 0;
        BOOL ok              = fn_acceptex(static_cast<SOCKET>(listen_fd), accept_sock, ext->addr_buf, 0, overlapped_ext::kAddrSlotSize, overlapped_ext::kAddrSlotSize, &bytes_received, reinterpret_cast<LPOVERLAPPED>(as_ov(ext.get())));
        if (!ok && WSAGetLastError() != WSA_IO_PENDING) {
            closesocket(accept_sock);
            return false;
        }
        ext.release();  // ownership transferred to IOCP
        return true;
    }

    bool async::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
        auto ext = std::make_unique<overlapped_ext>();
        memset(ext->overlapped, 0, sizeof(ext->overlapped));
        ext->request_id  = request_id;
        ext->type        = overlapped_ext::op_connect;
        ext->accept_sock = static_cast<uintptr_t>(static_cast<SOCKET>(fd));  // reuse accept_sock to store connect fd

        // ConnectEx requires a pre-bound socket.
        // Bind to the appropriate wildcard address matching the endpoint family.
        if (ep.get_family() == net::family::inet6) {
            sockaddr_in6 bind_addr {};
            bind_addr.sin6_family = AF_INET6;
            bind(static_cast<SOCKET>(fd), reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));
        } else {
            sockaddr_in bind_addr {};
            bind_addr.sin_family      = AF_INET;
            bind_addr.sin_addr.s_addr = INADDR_ANY;
            bind(static_cast<SOCKET>(fd), reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));
        }

        if (!m_connectex) {
            return false;
        }
        auto fn_connectex = reinterpret_cast<LPFN_CONNECTEX>(m_connectex);
        BOOL ok           = fn_connectex(static_cast<SOCKET>(fd), ep.addr(), ep.addrlen(), nullptr, 0, nullptr, reinterpret_cast<LPOVERLAPPED>(as_ov(ext.get())));
        if (!ok && WSAGetLastError() != WSA_IO_PENDING) {
            return false;
        }
        ext.release();  // ownership transferred to IOCP
        return true;
    }

    bool async::submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) {
        auto ext = std::make_unique<overlapped_ext>();
        memset(ext->overlapped, 0, sizeof(ext->overlapped));
        ext->request_id  = request_id;
        ext->type        = overlapped_ext::op_file_read;
        ext->accept_sock = 0;

        auto* ov       = as_ov(ext.get());
        ov->Offset     = static_cast<DWORD>(offset & 0xFFFFFFFF);
        ov->OffsetHigh = static_cast<DWORD>(offset >> 32);

        BOOL ok = ReadFile(static_cast<HANDLE>(fd), buffer, static_cast<DWORD>(len), nullptr, ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            return false;
        }
        ext.release();  // ownership transferred to IOCP
        return true;
    }

    bool async::submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) {
        auto ext = std::make_unique<overlapped_ext>();
        memset(ext->overlapped, 0, sizeof(ext->overlapped));
        ext->request_id  = request_id;
        ext->type        = overlapped_ext::op_file_write;
        ext->accept_sock = 0;

        auto* ov       = as_ov(ext.get());
        ov->Offset     = static_cast<DWORD>(offset & 0xFFFFFFFF);
        ov->OffsetHigh = static_cast<DWORD>(offset >> 32);

        BOOL ok = WriteFile(static_cast<HANDLE>(fd), buffer, static_cast<DWORD>(len), nullptr, ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            return false;
        }
        ext.release();  // ownership transferred to IOCP
        return true;
    }

    bool async::submit_poll(net::fd_t fd, uint64_t request_id) {
        // 使用 zero-byte WSARecv 实现 poll：不消费任何数据，
        // 当 fd 可读时 IOCP 会收到完成通知。
        auto ext = std::make_unique<overlapped_ext>();
        memset(ext->overlapped, 0, sizeof(ext->overlapped));
        ext->request_id  = request_id;
        ext->type        = overlapped_ext::op_poll;
        ext->accept_sock = 0;

        WSABUF buf;
        buf.buf = nullptr;
        buf.len = 0;

        DWORD flags = 0;
        DWORD bytes = 0;
        int rc      = WSARecv(static_cast<SOCKET>(fd), &buf, 1, &bytes, &flags, reinterpret_cast<LPWSAOVERLAPPED>(as_ov(ext.get())), nullptr);
        if (rc == 0) {
            m_sync_completions.push_back(make_sync_completion(std::move(ext), bytes));
            return true;
        }
        if (WSAGetLastError() != WSA_IO_PENDING) {
            return false;
        }
        ext.release();  // ownership transferred to IOCP
        return true;
    }

    static io_completion make_completion(OVERLAPPED_ENTRY& entry) {
        std::unique_ptr<async::overlapped_ext> ext(
            reinterpret_cast<async::overlapped_ext*>(entry.lpOverlapped)
        );
        auto* ov = reinterpret_cast<OVERLAPPED*>(ext->overlapped);
        io_completion c;
        c.request_id                       = ext->request_id;
        c.bytes_transferred                = static_cast<size_t>(entry.dwNumberOfBytesTransferred);
        c.error_code                       = 0;
        static constexpr async_op op_map[] = {
            async_op::read,
            async_op::write,
            async_op::accept,
            async_op::connect,
            async_op::file_read,
            async_op::file_write,
            async_op::fd_poll,
        };
        c.op = op_map[static_cast<int>(ext->type)];

        // OVERLAPPED.Internal holds the NTSTATUS of the completed I/O.
        // A non-zero value means the operation failed.
        DWORD err = 0;
        if (ov->Internal != 0) {
            // Convert NTSTATUS to Win32 error code.
            err = static_cast<DWORD>(ov->Internal);
            if ((err & 0xC0000000) == 0xC0000000) {
                // Try to get a proper Win32 error via RtlNtStatusToDosError.
                // For simplicity, mask off the NTSTATUS facility bits;
                // many NTSTATUS codes map directly to Win32 errors.
                err = RtlNtStatusToDosError(static_cast<NTSTATUS>(ov->Internal));
            }
        }

        if (ext->type == async::overlapped_ext::op_accept) {
            if (err == 0) {
                SOCKET as = static_cast<SOCKET>(ext->accept_sock);
                SOCKET ls = static_cast<SOCKET>(ext->listen_sock);
                // Inherit listen socket properties so getsockname/getpeername/shutdown work.
                if (setsockopt(as, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(&ls), sizeof(ls)) != 0) {
                    closesocket(as);
                    c.status     = async_status::error;
                    c.error_code = static_cast<int>(WSAGetLastError());
                    return c;
                }
                // Set non-blocking to match bee.socket.accept behaviour.
                unsigned long nonblock = 1;
                if (ioctlsocket(as, FIONBIO, &nonblock) != 0) {
                    closesocket(as);
                    c.status     = async_status::error;
                    c.error_code = static_cast<int>(WSAGetLastError());
                    return c;
                }
                c.status            = async_status::success;
                c.bytes_transferred = ext->accept_sock;  // new fd
            } else {
                closesocket(static_cast<SOCKET>(ext->accept_sock));
                c.status     = async_status::error;
                c.error_code = static_cast<int>(err);
            }
        } else if (ext->type == async::overlapped_ext::op_connect) {
            if (err == 0) {
                // Update the socket context so getsockname/shutdown/etc. work correctly.
                if (setsockopt(static_cast<SOCKET>(ext->accept_sock), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) != 0) {
                    c.status     = async_status::error;
                    c.error_code = static_cast<int>(WSAGetLastError());
                    return c;
                }
                c.status = async_status::success;
            } else {
                c.status     = async_status::error;
                c.error_code = static_cast<int>(err);
            }
        } else if (err != 0) {
            c.status     = async_status::error;
            c.error_code = static_cast<int>(err);
        } else if (entry.dwNumberOfBytesTransferred == 0 &&
                   ext->type != async::overlapped_ext::op_file_read &&
                   ext->type != async::overlapped_ext::op_file_write &&
                   ext->type != async::overlapped_ext::op_poll) {
            c.status = async_status::close;
        } else {
            c.status = async_status::success;
        }

        return c;
    }

    int async::poll(const span<io_completion>& completions) {
        int count = 0;
        // Drain synchronous completions first.
        while (!m_sync_completions.empty() && count < static_cast<int>(completions.size())) {
            completions[count++] = m_sync_completions.front();
            m_sync_completions.pop_front();
        }
        if (count >= static_cast<int>(completions.size())) return count;

        static_assert(sizeof(OVERLAPPED_ENTRY) > 0);
        const ULONG max = static_cast<ULONG>(completions.size()) - static_cast<ULONG>(count);
        dynarray<OVERLAPPED_ENTRY> entries(max);
        ULONG n = 0;
        BOOL ok = GetQueuedCompletionStatusEx(static_cast<HANDLE>(m_iocp), entries.data(), max, &n, 0, FALSE);
        if (ok && n > 0) {
            for (ULONG i = 0; i < n; ++i) {
                completions[count++] = make_completion(entries[i]);
            }
        }
        return count;
    }

    int async::wait(const span<io_completion>& completions, int timeout) {
        int count = 0;
        // Drain synchronous completions first — if any exist, return immediately
        // without blocking.
        while (!m_sync_completions.empty() && count < static_cast<int>(completions.size())) {
            completions[count++] = m_sync_completions.front();
            m_sync_completions.pop_front();
        }
        if (count > 0) return count;

        const ULONG max = static_cast<ULONG>(completions.size());
        dynarray<OVERLAPPED_ENTRY> entries(max);
        DWORD ms = (timeout < 0) ? INFINITE : static_cast<DWORD>(timeout);
        ULONG n  = 0;
        BOOL ok  = GetQueuedCompletionStatusEx(static_cast<HANDLE>(m_iocp), entries.data(), max, &n, ms, FALSE);
        if (!ok || n == 0) return 0;
        for (ULONG i = 0; i < n; ++i) {
            completions[i] = make_completion(entries[i]);
        }
        return static_cast<int>(n);
    }

    void async::stop() {
        if (!m_iocp) return;

        // Drain all completion notifications so every overlapped_ext is freed.
        // After CancelIoEx the cancelled operations will complete with
        // ERROR_OPERATION_ABORTED; we just need to delete each ext.
        static constexpr ULONG kDrainBatch = 64;
        OVERLAPPED_ENTRY entries[kDrainBatch];
        for (;;) {
            ULONG count = 0;
            BOOL ok     = GetQueuedCompletionStatusEx(static_cast<HANDLE>(m_iocp), entries, kDrainBatch, &count, 0, FALSE);
            if (!ok || count == 0) break;
            for (ULONG i = 0; i < count; ++i) {
                std::unique_ptr<async::overlapped_ext> ext(
                    reinterpret_cast<async::overlapped_ext*>(entries[i].lpOverlapped)
                );
                if (ext->type == async::overlapped_ext::op_accept) {
                    closesocket(static_cast<SOCKET>(ext->accept_sock));
                }
            }
        }

        CloseHandle(static_cast<HANDLE>(m_iocp));
        m_iocp = nullptr;
    }

}  // namespace bee::async
