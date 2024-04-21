#pragma once

#include <bee/utility/zstring_view.h>

#include <string>

namespace bee::wtf8 {
    std::wstring u2w(zstring_view str) noexcept;
    std::string w2u(wzstring_view wstr) noexcept;
}
