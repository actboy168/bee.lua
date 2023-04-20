#include <bee/subprocess.h>
#include <bee/subprocess/process_select.h>

#if defined(_WIN32)
#    include <Windows.h>
#else
#    include <poll.h>
#endif

namespace bee::subprocess {
    bool process_select(dynarray<process*> const& set) {
#if defined(_WIN32)
        if (set.size() >= MAXIMUM_WAIT_OBJECTS) {
            return false;
        }
        dynarray<HANDLE> fds(set.size());
        DWORD nfds = 0;
        for (auto const& p : set) {
            auto fd     = p->native_handle();
            fds[nfds++] = fd;
        }
        DWORD ret = WaitForMultipleObjects(nfds, fds.data(), FALSE, INFINITE);
        if (ret < WAIT_OBJECT_0 || ret > (WAIT_OBJECT_0 + nfds - 1)) {
            return false;
        }
        return true;
#else
        dynarray<pollfd> fds(set.size());
        nfds_t nfds = 0;
        for (auto const& p : set) {
            auto fd     = p->native_handle();
            fds[nfds++] = { fd, POLLIN | POLLPRI, 0 };
        }
        int ret = poll(fds.data(), nfds, -1);
        if (ret == -1) {
            return false;
        }
        return true;
#endif
    }
}
