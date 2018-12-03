#pragma once

#include <system_error>

namespace bee {
    const std::error_category& windows_category() noexcept;
}
