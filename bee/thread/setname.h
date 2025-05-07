#pragma once

#include <string_view>

namespace bee {
    void thread_setname(std::string_view name) noexcept;
}
