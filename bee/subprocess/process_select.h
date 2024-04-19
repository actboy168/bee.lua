#pragma once

#include <bee/subprocess.h>
#include <bee/utility/dynarray.h>

namespace bee::subprocess {
    enum class status {
        success,
        timeout,
        failed,
    };
    status process_select(const dynarray<process_handle>& set, int timeout) noexcept;
}
