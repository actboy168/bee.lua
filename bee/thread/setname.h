#pragma once

#include <bee/utility/zstring_view.h>

namespace bee {
    void thread_setname(zstring_view name) noexcept;
}
