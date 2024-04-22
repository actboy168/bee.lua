#include <Windows.h>
#include <bee/net/bpoll.h>
#include <bee/win/afd/afd.h>
#include <bee/win/afd/poller.h>

namespace bee::net {
    fd_t bpoll_create() {
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

    bool bpoll_close(fd_t fd) {
        if (fd == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)fd;
        delete ep;
        return true;
    }

    bool bpoll_ctl_add(fd_t fd, fd_t socket, const bpoll_event_t& event) {
        if (fd == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)fd;
        if (!ep->ctl_add(socket, event)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    bool bpoll_ctl_mod(fd_t fd, fd_t socket, const bpoll_event_t& event) {
        if (fd == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)fd;
        if (!ep->ctl_mod(socket, event)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    bool bpoll_ctl_del(fd_t fd, fd_t socket) {
        if (fd == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)fd;
        if (!ep->ctl_del(socket)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    int bpoll_wait(fd_t fd, const span<bpoll_event_t>& events, int timeout) {
        if (fd == retired_fd) {
            SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }
        auto ep = (afd::poller*)fd;
        return ep->wait(events, timeout);
    }
}
