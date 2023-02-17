#pragma once

#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/expected.h>

namespace bee::path_helper {
    using path_expected = expected<fs::path, std::string>;
    path_expected dll_path(void* module_handle);
    path_expected exe_path();
    path_expected dll_path();
    bool equal(fs::path const& lhs, fs::path const& rhs);
}
