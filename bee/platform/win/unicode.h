#pragma once

#include <string>
#include <bee/utility/zstring_view.h>

namespace bee::win {
    std::wstring u2w(zstring_view str);
    std::string w2u(wzstring_view wstr);
    std::wstring a2w(zstring_view str);
    std::string w2a(wzstring_view wstr);
    std::string a2u(zstring_view str);
    std::string u2a(zstring_view str);
}
