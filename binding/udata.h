#pragma once

#include <bee/nonstd/filesystem.h>
#include <binding/binding.h>

namespace bee::lua {
    template <>
    struct udata<fs::path> {
        static inline auto name = "bee::path";
    };
}
