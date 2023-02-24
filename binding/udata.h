#pragma once

#include <binding/binding.h>
#include <bee/nonstd/filesystem.h>

namespace bee::lua {
    template <>
    struct udata<fs::path> {
        static inline auto name = "bee::path";
    };
}
