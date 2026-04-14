#pragma once

#include <cstddef>
#include <cstdint>

namespace bee::async {

    enum class async_status : uint8_t {
        success,
        close,
        error,
        cancel,
    };

    enum class async_op : uint8_t {
        read,
        readv,
        write,
        writev,
        accept,
        connect,
        file_read,
        file_write,
        fd_poll,
    };

    struct io_completion {
        uint64_t request_id;
        async_status status;
        async_op op;
        size_t bytes_transferred;
        int error_code;
    };

}  // namespace bee::async
