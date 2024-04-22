#include <Windows.h>
#include <bee/win/afd/afd.h>
#include <bee/win/afd/poller.h>
#include <bee/net/bpoll.h>

namespace bee::net {
    bpoll_handle bpoll_create() {
        afd::afd_context ctx;
        if (!afd::afd_create(ctx)) {
            return INVALID_HANDLE_VALUE;
        }
        afd::poller* ep = new (std::nothrow) afd::poller(std::move(ctx));
        if (ep == NULL) {
            afd::afd_destroy(ctx);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return INVALID_HANDLE_VALUE;
        }
        return (bpoll_handle)ep;
    }

    bool bpoll_close(bpoll_handle hnd) {
        if (hnd == INVALID_HANDLE_VALUE) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)hnd;
        delete ep;
        return true;
    }

    bool bpoll_ctl_add(bpoll_handle hnd, bpoll_socket socket, const bpoll_event_t& event) {
        if (hnd == INVALID_HANDLE_VALUE) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)(hnd);
        if (!ep->ctl_add(socket, event)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    bool bpoll_ctl_mod(bpoll_handle hnd, bpoll_socket socket, const bpoll_event_t& event) {
        if (hnd == INVALID_HANDLE_VALUE) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)(hnd);
        if (!ep->ctl_mod(socket, event)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    bool bpoll_ctl_del(bpoll_handle hnd, bpoll_socket socket) {
        if (hnd == INVALID_HANDLE_VALUE) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        if (socket == 0 || socket == INVALID_SOCKET) {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }
        auto ep = (afd::poller*)(hnd);
        if (!ep->ctl_del(socket)) {
            DWORD flags;
            GetHandleInformation((HANDLE)socket, &flags);
            return false;
        }
        return true;
    }

    int bpoll_wait(bpoll_handle hnd, const span<bpoll_event_t>& events, int timeout) {
        if (hnd == INVALID_HANDLE_VALUE) {
            SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }
        auto ep = (afd::poller*)hnd;
        return ep->wait(events, timeout);
    }
}
