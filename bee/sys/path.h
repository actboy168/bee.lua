#pragma once

#include <bee/nonstd/filesystem.h>

#include <optional>

namespace bee::sys {
    std::optional<fs::path> exe_path() noexcept;
    std::optional<fs::path> dll_path() noexcept;
}
