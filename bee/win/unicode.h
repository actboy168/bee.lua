#pragma once

#include <bee/utility/zstring_view.h>

#include <string>

namespace bee::win {
    std::string a2u(zstring_view str) noexcept;
    std::string u2a(zstring_view str) noexcept;
}
