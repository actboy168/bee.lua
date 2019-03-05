#pragma once

#include <bee/config.h>
#include <bee/nonstd/expected.h>
#include <bee/filesystem.h>

namespace bee::path_helper {
    _BEE_API auto dll_path(void* module_handle)->nonstd::expected<fs::path, std::exception>;
    _BEE_API auto exe_path()->nonstd::expected<fs::path, std::exception>;
    _BEE_API auto dll_path()->nonstd::expected<fs::path, std::exception>;
    _BEE_API bool equal(fs::path const& lhs, fs::path const& rhs);
}
