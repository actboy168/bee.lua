#pragma once

#include <bee/utility/zstring_view.h>

#include <string>

namespace bee::win {
    std::wstring u2w(zstring_view str) noexcept;
    std::string w2u(wzstring_view wstr) noexcept;
    std::wstring a2w(zstring_view str) noexcept;
    std::string w2a(wzstring_view wstr) noexcept;
    std::string a2u(zstring_view str) noexcept;
    std::string u2a(zstring_view str) noexcept;
}
