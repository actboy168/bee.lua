#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>

namespace bee::async {

    // Per-stream receive ring buffer.
    //
    // cap is always a power of two so that index masking (& (cap-1)) works.
    // head and tail are monotonically increasing absolute offsets; the actual
    // position in data[] is obtained via (offset & (cap-1)).
    //
    // Invariants:
    //   0 <= size() <= cap
    //   write_ptr() points into the free region [tail, head+cap)
    //
    // Lifetime: held as a Lua userdata object; GC handles deallocation.
    // Thread safety: none -- accessed from a single Lua thread.
    struct ring_buf {
        char* data  = nullptr;
        size_t cap  = 0;  // capacity, always a power of two
        size_t head = 0;  // consumer cursor (absolute, never wraps)
        size_t tail = 0;  // producer cursor (absolute, never wraps)

        // --------------- capacity / state queries ---------------

        size_t size() const noexcept { return tail - head; }
        size_t free_cap() const noexcept { return cap - size(); }
        bool empty() const noexcept { return head == tail; }

        // --------------- producer side (used by submit_stream_read) ---------------

        // Pointer to the start of the contiguous free region.
        char* write_ptr() noexcept {
            return data + (tail & (cap - 1));
        }

        // Length of the contiguous free region.  May be less than free_cap() when
        // the free space wraps around the end of the buffer.
        size_t write_len() const noexcept {
            if (free_cap() == 0) return 0;
            size_t wrap_end = cap - (tail & (cap - 1));
            return (std::min)(wrap_end, free_cap());
        }

        // Advance the producer cursor after a successful read completion.
        void commit(size_t n) noexcept {
            assert(n <= free_cap());
            tail += n;
        }

        // --------------- consumer side (used by read) ---------------

        // Search for the first occurrence of the byte sequence [sep, sep+seplen) in
        // the buffered data.  Returns the number of bytes from head up to and
        // including the last byte of the found sequence, or 0 if not found.
        // seplen must be >= 1.
        size_t find(const char* sep, size_t seplen) const noexcept {
            size_t n = size();
            if (n < seplen) return 0;
            size_t limit = n - seplen + 1;
            for (size_t i = 0; i < limit; ++i) {
                bool match = true;
                for (size_t j = 0; j < seplen; ++j) {
                    if (data[(head + i + j) & (cap - 1)] != sep[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return i + seplen;
            }
            return 0;
        }

        // Copy exactly n bytes from the ring into dst and advance head.
        // Returns false (without modifying head) if fewer than n bytes are available.
        bool consume(char* dst, size_t n) noexcept {
            if (size() < n) return false;
            size_t idx   = head & (cap - 1);
            size_t first = (std::min)(n, cap - idx);
            memcpy(dst, data + idx, first);
            if (first < n) {
                memcpy(dst + first, data, n - first);
            }
            head += n;
            return true;
        }

        // --------------- lifecycle ---------------

        // Round sz up to the nearest power of two (minimum 16).
        static size_t round_up_pow2(size_t sz) noexcept {
            if (sz < 16) sz = 16;
            sz--;
            sz |= sz >> 1;
            sz |= sz >> 2;
            sz |= sz >> 4;
            sz |= sz >> 8;
            sz |= sz >> 16;
            if constexpr (sizeof(size_t) > 4) {
                sz |= sz >> 32;
            }
            return sz + 1;
        }

        explicit ring_buf(size_t bufsize) {
            cap  = round_up_pow2(bufsize);
            data = new char[cap];
        }

        ring_buf() = default;

        ~ring_buf() noexcept {
            delete[] data;
        }
    };

}  // namespace bee::async
