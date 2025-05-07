#pragma once

#include <string_view>

namespace bee {
#if defined(_WIN32)
    using path_view = std::wstring_view;
#else
    using path_view = std::string_view;
#endif
}
