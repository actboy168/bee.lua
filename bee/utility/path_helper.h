#pragma once

#include <bee/config.h>
#include <bee/filesystem.h>
#include <bee/utility/expected.h>

namespace bee::path_helper {
    using path_expected = expected<fs::path, std::string>;
    _BEE_API path_expected dll_path(void* module_handle);
    _BEE_API path_expected exe_path();
    _BEE_API path_expected dll_path();
    _BEE_API path_expected appdata_path();
    _BEE_API bool equal(fs::path const& lhs, fs::path const& rhs);
}
