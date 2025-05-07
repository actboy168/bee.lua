#pragma once

#include <string>
#include <string_view>

namespace bee::wtf8 {
    std::wstring u2w(std::string_view str) noexcept;
    std::string w2u(std::wstring_view wstr) noexcept;
}
