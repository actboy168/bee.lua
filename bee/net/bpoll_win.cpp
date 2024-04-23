#include <Windows.h>
#include <bee/net/bpoll.h>
#include <bee/win/afd/afd.h>
#include <bee/win/afd/poller.h>

namespace bee::net {
    bpoll_handle bpoll_create() noexcept {
        afd::afd_context ctx;
        if (!afd::afd_create(ctx)) {
            return retired_fd;
        }
        afd::poller* ep = new (std::nothrow) afd::poller(std::move(ctx));
        if (ep == NULL) {
            afd::afd_destroy(ctx);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return retired_fd;
        }
        return (fd_t)ep;
    }

    bool bpoll_close(bpoll_handle handle) noexcept {
        if (handle == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)handle;
        delete ep;
        return true;
    }

    bool bpoll_ctl_add(bpoll_handle handle, fd_t socket, const bpoll_event_t& event) noexcept {
        if (handle == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)handle;
        if (!ep->ctl_add(socket, event)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    bool bpoll_ctl_mod(bpoll_handle handle, fd_t socket, const bpoll_event_t& event) noexcept {
        if (handle == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)handle;
        if (!ep->ctl_mod(socket, event)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    bool bpoll_ctl_del(bpoll_handle handle, fd_t socket) noexcept {
        if (handle == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)handle;
        if (!ep->ctl_del(socket)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    int bpoll_wait(bpoll_handle handle, const span<bpoll_event_t>& events, int timeout) noexcept {
        if (handle == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }
        auto ep = (afd::poller*)handle;
        return ep->wait(events, timeout);
    }
}
