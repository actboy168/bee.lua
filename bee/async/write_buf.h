#pragma once

#include <bee/net/socket.h>

#include <cstddef>
#include <cstdint>
#include <deque>

namespace bee::async {

    // Per-stream write buffer.
    //
    // Maintains a queue of pending string entries to be sent over a socket.
    // The C layer submits entries one at a time via submit_write, handling
    // partial writes transparently (auto-drain). When the queue drains to empty
    // a single Lua-visible completion is produced.
    //
    // hwm (high-water mark): write() returns true when buffered >= hwm,
    // signalling the Lua caller to back-pressure (block until stream_on_write).
    //
    // Lifetime: held as a Lua userdata object; GC handles deallocation.
    // Thread safety: none -- accessed from a single Lua thread.
    struct write_buf {
        struct entry {
            const char* data;  // pointer into Lua string (kept alive by str_ref)
            size_t len;
            size_t offset;  // bytes already sent (partial write progress)
            int str_ref;    // luaL_ref keeping the Lua string alive
        };

        std::deque<entry> q;
        size_t buffered = 0;      // total queued bytes
        size_t hwm      = 0;      // high-water mark threshold
        bool in_flight  = false;  // true while a writev is outstanding
        // Fields valid only while in_flight == true:
        net::fd_t fd       = net::fd_t {};  // socket being written to
        uint64_t lua_reqid = 0;             // Lua-assigned reqid for final completion
    };

}  // namespace bee::async
