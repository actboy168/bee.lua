#pragma once

#include <bee/utility/dynarray.h>

namespace bee::subprocess {
    class process;
    bool process_select(dynarray<process*> const& set);
}

