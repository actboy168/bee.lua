#pragma once

#include <bee/config.h>
#include <filesystem>
#include <bee/nonstd/expected.h>
#if defined(_WIN32)
#   include <Windows.h>
#endif

namespace bee::path_helper {
    namespace fs = std::filesystem;
#if defined(_WIN32)
    _BEE_API auto dll_path(HMODULE module_handle)->nonstd::expected<fs::path, std::exception>;
#endif
    _BEE_API auto exe_path()->nonstd::expected<fs::path, std::exception>;
    _BEE_API auto dll_path()->nonstd::expected<fs::path, std::exception>;
    _BEE_API bool equal(fs::path const& lhs, fs::path const& rhs);
}
