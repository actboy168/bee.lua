#pragma once

#include <bee/utility/dynarray.h>

namespace bee::subprocess {
    class process;
    enum class status {
        success,
        timeout,
        failed,
    };
    status process_select(dynarray<process*> const& set, int timeout);
}
