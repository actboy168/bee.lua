#pragma once

#include <string>
#include <string_view>

namespace bee::win {
    std::string a2u(std::string_view str) noexcept;
    std::string u2a(std::string_view str) noexcept;
}
