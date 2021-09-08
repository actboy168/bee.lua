#pragma once

#include <bee/config.h>
#include <bee/filesystem.h>

namespace bee::path_helper {
    _BEE_API fs::path dll_path(void* module_handle);
    _BEE_API fs::path exe_path();
    _BEE_API fs::path dll_path();
    _BEE_API fs::path appdata_path();
    _BEE_API bool equal(fs::path const& lhs, fs::path const& rhs);
}
