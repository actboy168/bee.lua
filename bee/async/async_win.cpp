// clang-format off
// WinSock2.h must be included before Windows.h to avoid winsock.h type redefinitions.
#include <WinSock2.h>
// clang-format on
#include <Windows.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

#include <bee/async/async_win.h>
#include <bee/async/async.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>

#include <cstring>

// RtlNtStatusToDosError is in ntdll but not declared in standard MinGW headers.
extern "C" ULONG WINAPI RtlNtStatusToDosError(NTSTATUS Status);

namespace bee::async {

// sizeof(OVERLAPPED) is 32 on both x86 and x64.
static_assert(sizeof(OVERLAPPED) <= sizeof(overlapped_ext::overlapped),
    "overlapped_ext::overlapped buffer is too small");

static inline OVERLAPPED* as_ov(overlapped_ext* ext) {
    return reinterpret_cast<OVERLAPPED*>(ext->overlapped);
}

async::async()
    : m_iocp(nullptr) {
    m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
}

async::~async() {
    stop();
}

bool async::ensure_associated(void* h) {
    auto key = reinterpret_cast<uintptr_t>(h);
    if (m_associated.count(key)) {
        return true;
    }
    if (CreateIoCompletionPort(static_cast<HANDLE>(h),
                               static_cast<HANDLE>(m_iocp), 0, 0) != m_iocp) {
        return false;
    }
    m_associated.insert(key);
    return true;
}

bool async::submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id) {
    if (!ensure_associated(reinterpret_cast<void*>(fd))) return false;
    auto* ext       = new overlapped_ext();
    memset(ext->overlapped, 0, sizeof(ext->overlapped));
    ext->request_id = request_id;
    ext->type       = overlapped_ext::op_read;
    ext->accept_sock = 0;

    WSABUF buf;
    buf.buf = static_cast<char*>(buffer);
    buf.len = static_cast<ULONG>(len);

    DWORD flags = 0;
    int rc = WSARecv(static_cast<SOCKET>(fd), &buf, 1, nullptr, &flags,
                     reinterpret_cast<LPWSAOVERLAPPED>(as_ov(ext)), nullptr);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        delete ext;
        return false;
    }
    return true;
}

bool async::submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id) {
    if (!ensure_associated(reinterpret_cast<void*>(fd))) return false;
    auto* ext       = new overlapped_ext();
    memset(ext->overlapped, 0, sizeof(ext->overlapped));
    ext->request_id = request_id;
    ext->type       = overlapped_ext::op_write;
    ext->accept_sock = 0;

    WSABUF buf;
    buf.buf = const_cast<char*>(static_cast<const char*>(buffer));
    buf.len = static_cast<ULONG>(len);

    int rc = WSASend(static_cast<SOCKET>(fd), &buf, 1, nullptr, 0,
                     reinterpret_cast<LPWSAOVERLAPPED>(as_ov(ext)), nullptr);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        delete ext;
        return false;
    }
    return true;
}

bool async::submit_accept(net::fd_t listen_fd, uint64_t request_id) {
    if (!ensure_associated(reinterpret_cast<void*>(listen_fd))) return false;

    // Query the address family of the listen socket so the accept socket matches.
    net::endpoint ep;
    int af = AF_INET;
    if (net::socket::getsockname(listen_fd, ep)) {
        if (ep.get_family() == net::family::inet6) {
            af = AF_INET6;
        }
    }
    SOCKET accept_sock = WSASocketW(af, SOCK_STREAM, IPPROTO_TCP,
                                    nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (accept_sock == INVALID_SOCKET) {
        return false;
    }

    auto* ext        = new overlapped_ext();
    memset(ext->overlapped, 0, sizeof(ext->overlapped));
    memset(ext->addr_buf, 0, sizeof(ext->addr_buf));
    ext->request_id  = request_id;
    ext->type        = overlapped_ext::op_accept;
    ext->accept_sock = static_cast<uintptr_t>(accept_sock);

    // AcceptEx output buffer: local addr slot + remote addr slot.
    // dwReceiveDataLength = 0 (no data prefix).
    DWORD bytes_received = 0;
    BOOL ok = AcceptEx(static_cast<SOCKET>(listen_fd), accept_sock,
                       ext->addr_buf, 0,
                       overlapped_ext::kAddrSlotSize, overlapped_ext::kAddrSlotSize,
                       &bytes_received, reinterpret_cast<LPOVERLAPPED>(as_ov(ext)));
    if (!ok && WSAGetLastError() != WSA_IO_PENDING) {
        closesocket(accept_sock);
        delete ext;
        return false;
    }
    return true;
}

bool async::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
    if (!ensure_associated(reinterpret_cast<void*>(fd))) return false;
    auto* ext       = new overlapped_ext();
    memset(ext->overlapped, 0, sizeof(ext->overlapped));
    ext->request_id = request_id;
    ext->type       = overlapped_ext::op_connect;
    ext->accept_sock = 0;

    // ConnectEx requires a pre-bound socket.
    // Bind to the appropriate wildcard address matching the endpoint family.
    if (ep.get_family() == net::family::inet6) {
        sockaddr_in6 bind_addr{};
        bind_addr.sin6_family = AF_INET6;
        bind(static_cast<SOCKET>(fd), reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));
    } else {
        sockaddr_in bind_addr{};
        bind_addr.sin_family      = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind(static_cast<SOCKET>(fd), reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));
    }

    BOOL ok = ConnectEx(static_cast<SOCKET>(fd), ep.addr(), ep.addrlen(),
                        nullptr, 0, nullptr,
                        reinterpret_cast<LPOVERLAPPED>(as_ov(ext)));
    if (!ok && WSAGetLastError() != WSA_IO_PENDING) {
        delete ext;
        return false;
    }
    return true;
}

bool async::submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) {
    if (!ensure_associated(fd)) return false;
    auto* ext       = new overlapped_ext();
    memset(ext->overlapped, 0, sizeof(ext->overlapped));
    ext->request_id = request_id;
    ext->type       = overlapped_ext::op_file_read;
    ext->accept_sock = 0;

    auto* ov       = as_ov(ext);
    ov->Offset     = static_cast<DWORD>(offset & 0xFFFFFFFF);
    ov->OffsetHigh = static_cast<DWORD>(offset >> 32);

    BOOL ok = ReadFile(static_cast<HANDLE>(fd), buffer, static_cast<DWORD>(len),
                       nullptr, ov);
    if (!ok && GetLastError() != ERROR_IO_PENDING) {
        delete ext;
        return false;
    }
    return true;
}

bool async::submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) {
    if (!ensure_associated(fd)) return false;
    auto* ext       = new overlapped_ext();
    memset(ext->overlapped, 0, sizeof(ext->overlapped));
    ext->request_id = request_id;
    ext->type       = overlapped_ext::op_file_write;
    ext->accept_sock = 0;

    auto* ov       = as_ov(ext);
    ov->Offset     = static_cast<DWORD>(offset & 0xFFFFFFFF);
    ov->OffsetHigh = static_cast<DWORD>(offset >> 32);

    BOOL ok = WriteFile(static_cast<HANDLE>(fd), buffer, static_cast<DWORD>(len),
                        nullptr, ov);
    if (!ok && GetLastError() != ERROR_IO_PENDING) {
        delete ext;
        return false;
    }
    return true;
}

static io_completion make_completion(OVERLAPPED_ENTRY& entry) {
    auto* ext = reinterpret_cast<overlapped_ext*>(entry.lpOverlapped);
    auto* ov  = reinterpret_cast<OVERLAPPED*>(ext->overlapped);
    io_completion c;
    c.request_id        = ext->request_id;
    c.bytes_transferred = static_cast<size_t>(entry.dwNumberOfBytesTransferred);
    c.error_code        = 0;
    static constexpr async_op op_map[] = {
        async_op::read, async_op::write, async_op::accept,
        async_op::connect, async_op::file_read, async_op::file_write,
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

    if (ext->type == overlapped_ext::op_accept) {
        if (err == 0) {
            c.status            = async_status::success;
            c.bytes_transferred = ext->accept_sock;  // new fd
        } else {
            closesocket(static_cast<SOCKET>(ext->accept_sock));
            c.status     = async_status::error;
            c.error_code = static_cast<int>(err);
        }
    } else if (err != 0) {
        c.status     = async_status::error;
        c.error_code = static_cast<int>(err);
    } else if (entry.dwNumberOfBytesTransferred == 0 &&
               ext->type != overlapped_ext::op_connect &&
               ext->type != overlapped_ext::op_file_read &&
               ext->type != overlapped_ext::op_file_write) {
        c.status = async_status::close;
    } else {
        c.status = async_status::success;
    }

    delete ext;
    return c;
}

int async::poll(const span<io_completion>& completions) {
    static_assert(sizeof(OVERLAPPED_ENTRY) > 0);
    ULONG count = 0;
    // Stack-allocate a temporary array to hold IOCP entries.
    const ULONG max = static_cast<ULONG>(completions.size());
    auto* entries = static_cast<OVERLAPPED_ENTRY*>(
        _alloca(max * sizeof(OVERLAPPED_ENTRY)));
    BOOL ok = GetQueuedCompletionStatusEx(static_cast<HANDLE>(m_iocp),
                                          entries, max, &count, 0, FALSE);
    if (!ok || count == 0) return 0;
    for (ULONG i = 0; i < count; ++i) {
        completions[i] = make_completion(entries[i]);
    }
    return static_cast<int>(count);
}

int async::wait(const span<io_completion>& completions, int timeout) {
    ULONG count = 0;
    const ULONG max = static_cast<ULONG>(completions.size());
    auto* entries = static_cast<OVERLAPPED_ENTRY*>(
        _alloca(max * sizeof(OVERLAPPED_ENTRY)));
    DWORD ms = (timeout < 0) ? INFINITE : static_cast<DWORD>(timeout);
    BOOL ok = GetQueuedCompletionStatusEx(static_cast<HANDLE>(m_iocp),
                                          entries, max, &count, ms, FALSE);
    if (!ok || count == 0) return 0;
    for (ULONG i = 0; i < count; ++i) {
        completions[i] = make_completion(entries[i]);
    }
    return static_cast<int>(count);
}

void async::stop() {
    if (!m_iocp) return;

    // Cancel all pending overlapped I/O on every associated handle.
    for (auto key : m_associated) {
        CancelIoEx(reinterpret_cast<HANDLE>(key), nullptr);
    }

    // Drain all completion notifications so every overlapped_ext is freed.
    // After CancelIoEx the cancelled operations will complete with
    // ERROR_OPERATION_ABORTED; we just need to delete each ext.
    static constexpr ULONG kDrainBatch = 64;
    OVERLAPPED_ENTRY entries[kDrainBatch];
    for (;;) {
        ULONG count = 0;
        BOOL ok = GetQueuedCompletionStatusEx(static_cast<HANDLE>(m_iocp),
                                              entries, kDrainBatch, &count, 0, FALSE);
        if (!ok || count == 0) break;
        for (ULONG i = 0; i < count; ++i) {
            auto* ext = reinterpret_cast<overlapped_ext*>(entries[i].lpOverlapped);
            if (ext->type == overlapped_ext::op_accept) {
                closesocket(static_cast<SOCKET>(ext->accept_sock));
            }
            delete ext;
        }
    }

    CloseHandle(static_cast<HANDLE>(m_iocp));
    m_iocp = nullptr;
    m_associated.clear();
}

}  // namespace bee::async
