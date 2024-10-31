/* clang-format off */
#include <winsock2.h>
#include <Windows.h>
/* clang-format on */

#include <bee/net/bpoll.h>
#include <bee/win/afd/afd.h>

#include <cassert>

namespace bee::net::afd {
    typedef LONG NTSTATUS;

    constexpr NTSTATUS NTSTATUS_SUCCESS   = 0x00000000L;
    constexpr NTSTATUS NTSTATUS_PENDING   = 0x00000103L;
    constexpr NTSTATUS NTSTATUS_CANCELLED = 0xC0000120L;
    constexpr NTSTATUS NTSTATUS_NOT_FOUND = 0xC0000225L;

    constexpr uint32_t AFD_POLL_RECEIVE           = 0x0001;
    constexpr uint32_t AFD_POLL_RECEIVE_EXPEDITED = 0x0002;
    constexpr uint32_t AFD_POLL_SEND              = 0x0004;
    constexpr uint32_t AFD_POLL_DISCONNECT        = 0x0008;
    constexpr uint32_t AFD_POLL_ABORT             = 0x0010;
    constexpr uint32_t AFD_POLL_LOCAL_CLOSE       = 0x0020;
    constexpr uint32_t AFD_POLL_ACCEPT            = 0x0080;
    constexpr uint32_t AFD_POLL_CONNECT_FAIL      = 0x0100;

    constexpr uint32_t kFILE_OPEN           = 0x00000001;
    constexpr uint32_t kIOCTL_AFD_POLL      = 0x00012024;
    constexpr uint32_t kSIO_BSP_HANDLE_POLL = 0x4800001D;
    constexpr uint32_t kSIO_BASE_HANDLE     = 0x48000022;

    struct UNICODE_STRING {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    };

    struct OBJECT_ATTRIBUTES {
        ULONG Length;
        HANDLE RootDirectory;
        UNICODE_STRING* ObjectName;
        ULONG Attributes;
        PVOID SecurityDescriptor;
        PVOID SecurityQualityOfService;
    };

#define RTL_CONSTANT_STRING(s) \
    { sizeof(s) - sizeof((s)[0]), sizeof(s), (PWSTR)s }
#define RTL_CONSTANT_OBJECT_ATTRIBUTES(ObjectName, Attributes) \
    { sizeof(OBJECT_ATTRIBUTES), NULL, ObjectName, Attributes, NULL, NULL }

    static UNICODE_STRING afd_device_name          = RTL_CONSTANT_STRING(L"\\Device\\Afd\\bee\\epoll");
    static OBJECT_ATTRIBUTES afd_device_attributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(&afd_device_name, 0);

    static uint32_t bpoll_events_to_afd_events(bpoll_event bpoll_events) noexcept {
        uint32_t afd_events = AFD_POLL_LOCAL_CLOSE;
        if (bitmask_has(bpoll_events, bpoll_event::in | bpoll_event::rdnorm))
            afd_events |= AFD_POLL_RECEIVE | AFD_POLL_ACCEPT;
        if (bitmask_has(bpoll_events, bpoll_event::pri | bpoll_event::rdband))
            afd_events |= AFD_POLL_RECEIVE_EXPEDITED;
        if (bitmask_has(bpoll_events, bpoll_event::out | bpoll_event::wrnorm | bpoll_event::wrand))
            afd_events |= AFD_POLL_SEND;
        if (bitmask_has(bpoll_events, bpoll_event::in | bpoll_event::rdnorm | bpoll_event::rdhup))
            afd_events |= AFD_POLL_DISCONNECT;
        if (bitmask_has(bpoll_events, bpoll_event::hup))
            afd_events |= AFD_POLL_ABORT;
        if (bitmask_has(bpoll_events, bpoll_event::err))
            afd_events |= AFD_POLL_CONNECT_FAIL;
        return afd_events;
    }

    static bpoll_event afd_events_to_bpoll_events(uint32_t afd_events) noexcept {
        bpoll_event bpoll_events = bpoll_event::null;
        if (afd_events & (AFD_POLL_RECEIVE | AFD_POLL_ACCEPT))
            bpoll_events |= bpoll_event::in | bpoll_event::rdnorm;
        if (afd_events & AFD_POLL_RECEIVE_EXPEDITED)
            bpoll_events |= bpoll_event::pri | bpoll_event::rdband;
        if (afd_events & AFD_POLL_SEND)
            bpoll_events |= bpoll_event::out | bpoll_event::wrnorm | bpoll_event::wrand;
        if (afd_events & AFD_POLL_DISCONNECT)
            bpoll_events |= bpoll_event::in | bpoll_event::rdnorm | bpoll_event::rdhup;
        if (afd_events & AFD_POLL_ABORT)
            bpoll_events |= bpoll_event::hup;
        if (afd_events & AFD_POLL_CONNECT_FAIL)
            bpoll_events |= bpoll_event::in | bpoll_event::out | bpoll_event::err | bpoll_event::rdnorm | bpoll_event::wrnorm | bpoll_event::rdhup;
        return bpoll_events;
    }

    extern "C" {
    typedef VOID(NTAPI* PIO_APC_ROUTINE)(PVOID ApcContext, IO_STATUS_BLOCK* IoStatusBlock, ULONG Reserved);
    NTSTATUS NTAPI NtCancelIoFileEx(HANDLE FileHandle, IO_STATUS_BLOCK* IoRequestToCancel, IO_STATUS_BLOCK* IoStatusBlock);
    NTSTATUS NTAPI NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, OBJECT_ATTRIBUTES* ObjectAttributes, IO_STATUS_BLOCK* IoStatusBlock, PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength);
    NTSTATUS NTAPI NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, IO_STATUS_BLOCK* IoStatusBlock, ULONG IoControlCode, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength);
    ULONG WINAPI RtlNtStatusToDosError(NTSTATUS Status);
    }

    bool afd_create(afd_context& ctx) noexcept {
        ctx.iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (ctx.iocp_handle == NULL) {
            return false;
        }
        IO_STATUS_BLOCK iosb;
        NTSTATUS status = NtCreateFile(&ctx.afd_handle, SYNCHRONIZE, &afd_device_attributes, &iosb, NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, kFILE_OPEN, 0, NULL, 0);
        if (status != NTSTATUS_SUCCESS) {
            CloseHandle(ctx.iocp_handle);
            SetLastError(RtlNtStatusToDosError(status));
            return false;
        }
        if (CreateIoCompletionPort(ctx.afd_handle, ctx.iocp_handle, 0, 0) == NULL) {
            CloseHandle(ctx.afd_handle);
            CloseHandle(ctx.iocp_handle);
            return false;
        }
        if (!SetFileCompletionNotificationModes(ctx.afd_handle, FILE_SKIP_SET_EVENT_ON_HANDLE)) {
            CloseHandle(ctx.afd_handle);
            CloseHandle(ctx.iocp_handle);
            return false;
        }
        return true;
    }

    void afd_destroy(afd_context& ctx) noexcept {
        CloseHandle(ctx.iocp_handle);
        CloseHandle(ctx.afd_handle);
    }

    uint32_t afd_wait(const afd_context& ctx, const span<_OVERLAPPED_ENTRY>& events, uint32_t timeout) noexcept {
        ULONG count;
        if (!GetQueuedCompletionStatusEx(ctx.iocp_handle, events.data(), (ULONG)events.size(), &count, timeout, FALSE)) {
            return (uint32_t)-1;
        }
        return (uint32_t)count;
    }

    bool afd_poll(const afd_context& ctx, afd_poll_context& poll) noexcept {
        poll.io_status_block.Status = NTSTATUS_PENDING;
        poll.afd_poll_info.Events   = bpoll_events_to_afd_events(static_cast<bpoll_event>(poll.afd_poll_info.Events));
        NTSTATUS status             = NtDeviceIoControlFile(ctx.afd_handle, NULL, NULL, &poll.io_status_block, &poll.io_status_block, kIOCTL_AFD_POLL, &poll.afd_poll_info, sizeof(AFD_POLL_INFO), &poll.afd_poll_info, sizeof(AFD_POLL_INFO));
        if (status == NTSTATUS_SUCCESS) {
            return true;
        }
        SetLastError(RtlNtStatusToDosError(status));
        return false;
    }

    bool afd_cancel_poll(const afd_context& ctx, afd_poll_context& poll) noexcept {
        NTSTATUS cancel_status;
        IO_STATUS_BLOCK cancel_iosb;
        if (poll.io_status_block.Status != STATUS_PENDING) {
            return true;
        }
        cancel_status = NtCancelIoFileEx(ctx.afd_handle, &poll.io_status_block, &cancel_iosb);
        if (cancel_status == NTSTATUS_SUCCESS || cancel_status == NTSTATUS_NOT_FOUND) {
            return true;
        }
        SetLastError(RtlNtStatusToDosError(cancel_status));
        return false;
    }

    afd_poll_event afd_query_event(const afd_poll_context& poll) noexcept {
        if (poll.io_status_block.Status == NTSTATUS_CANCELLED) {
            return { afd_poll_status::cancelled };
        } else if (poll.io_status_block.Status < 0) {
            return { afd_poll_status::failed, bpoll_event::err };
        } else if (poll.afd_poll_info.NumberOfHandles < 1) {
            return { afd_poll_status::nothing };
        } else if (poll.afd_poll_info.Events & AFD_POLL_LOCAL_CLOSE) {
            return { afd_poll_status::closed };
        } else {
            uint32_t afd_events = poll.afd_poll_info.Events;
            return { afd_poll_status::succeeded, afd_events_to_bpoll_events(afd_events) };
        }
    }

    static fd_t get_bsp_socket(fd_t socket, DWORD ioctl) noexcept {
        fd_t bsp_socket;
        DWORD bytes;
        if (WSAIoctl(socket, ioctl, NULL, 0, &bsp_socket, sizeof(bsp_socket), &bytes, NULL, NULL) == SOCKET_ERROR) {
            return INVALID_SOCKET;
        }
        return bsp_socket;
    }

    fd_t afd_get_base_socket(fd_t socket) noexcept {
        fd_t base_socket;
        DWORD error;
        for (;;) {
            base_socket = get_bsp_socket(socket, kSIO_BASE_HANDLE);
            if (base_socket != INVALID_SOCKET) {
                return base_socket;
            }
            error = GetLastError();
            if (error == WSAENOTSOCK) {
                SetLastError(error);
                return INVALID_SOCKET;
            }
            base_socket = get_bsp_socket(socket, kSIO_BSP_HANDLE_POLL);
            if (base_socket == INVALID_SOCKET || base_socket == socket) {
                SetLastError(error);
                return INVALID_SOCKET;
            }
            socket = base_socket;
        }
    }
}
