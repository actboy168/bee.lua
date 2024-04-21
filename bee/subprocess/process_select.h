#pragma once

#include <bee/subprocess.h>
#include <bee/utility/span.h>

namespace bee::subprocess {
    enum class status {
        success,
        timeout,
        failed,
    };
    status process_select(const span<process_handle>& set, int timeout) noexcept;
}
