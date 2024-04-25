#pragma once

#include <bee/utility/zstring_view.h>

namespace bee {
#if defined(_WIN32)
    using path_view = wzstring_view;
#else
    using path_view = zstring_view;
#endif
}
