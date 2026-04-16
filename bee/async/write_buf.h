#pragma once

#include <bee/net/socket.h>
#include <bee/utility/dynarray.h>
#include <bee/utility/span.h>

#include <cstddef>
#include <cstdint>
#include <deque>

namespace bee::async {

    // Per-stream write buffer.
    //
    // Maintains a queue of pending string entries to be sent over a socket.
    // The C layer submits all queued entries at once via submit_write using
    // submit_write with multiple iovecs, handling partial writes transparently
    // (auto-drain). When the queue drains to empty a single Lua-visible
    // completion is produced.
    //
    // hwm (high-water mark): enqueue() returns true when buffered >= hwm,
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

        // --------------- queries ---------------

        bool empty() const noexcept { return q_.empty(); }

        // Returns true when no new write should be submitted right now:
        // either a write is already outstanding, or the queue is empty.
        bool idle() const noexcept { return in_flight_ || q_.empty(); }

        size_t get_buffered() const noexcept { return buffered_; }

        bool over_hwm() const noexcept { return buffered_ >= hwm_; }

        net::fd_t target_fd() const noexcept { return fd_; }

        uint64_t request_id() const noexcept { return lua_reqid_; }

        // --------------- configuration ---------------

        void set_hwm(size_t hwm) noexcept { hwm_ = hwm; }

        void set_target(net::fd_t fd, uint64_t reqid) noexcept {
            fd_        = fd;
            lua_reqid_ = reqid;
        }

        // --------------- producer side ---------------

        // Enqueue a string entry. Returns true when buffered >= hwm (back-pressure signal).
        bool enqueue(const char* data, size_t len, int str_ref) {
            q_.push_back({ data, len, 0, str_ref });
            buffered_ += len;
            return buffered_ >= hwm_;
        }

        // --------------- submit / flight ---------------

        // Build an iov snapshot from the current queue and return it as a span.
        // The returned span is valid until the next call to build_iov() or clear().
        span<const net::socket::iobuf> build_iov() {
            size_t n  = q_.size();
            iov_cache_ = dynarray<net::socket::iobuf>(n);
            size_t i  = 0;
            for (auto& e : q_) {
                iov_cache_[i++].set(e.data + e.offset, e.len - e.offset);
            }
            return { iov_cache_.data(), n };
        }

        void begin_flight() noexcept { in_flight_ = true; }
        void end_flight() noexcept { in_flight_ = false; }

        // --------------- completion / consume ---------------

        // Consume bytes_transferred from the front of the queue, honouring
        // partial writes across multiple entries.
        // unref_fn is called for each fully consumed entry: unref_fn(str_ref).
        template <typename UnrefFn>
        void consume(size_t bytes, UnrefFn&& unref_fn) {
            size_t remaining = bytes;
            while (!q_.empty() && remaining > 0) {
                entry& front = q_.front();
                size_t avail = front.len - front.offset;
                if (remaining >= avail) {
                    remaining -= avail;
                    unref_fn(front.str_ref);
                    buffered_ -= front.len;
                    q_.pop_front();
                } else {
                    front.offset += remaining;
                    remaining = 0;
                }
            }
        }

        // Release all queued entries and reset state.
        // unref_fn is called for each entry: unref_fn(str_ref).
        template <typename UnrefFn>
        void clear(UnrefFn&& unref_fn) {
            for (auto& e : q_) unref_fn(e.str_ref);
            q_.clear();
            buffered_ = 0;
        }

    private:
        std::deque<entry> q_;
        size_t buffered_  = 0;      // total queued bytes
        size_t hwm_       = 0;      // high-water mark threshold
        bool in_flight_   = false;  // true while a write is outstanding
        // Fields valid only while in_flight_ == true:
        net::fd_t fd_        = net::fd_t {};  // socket being written to
        uint64_t lua_reqid_  = 0;             // Lua-assigned reqid for final completion
        // iov snapshot for the current in-flight write (entries may span multiple q items).
        // Built by build_iov(); must remain valid until the completion callback fires.
        dynarray<net::socket::iobuf> iov_cache_;
    };

}  // namespace bee::async
