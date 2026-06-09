#pragma once

#include <cstdint>
#include <set>
#include <string_view>

namespace bee::win {
    std::set<uint32_t> find_file_holders(std::wstring_view filepath);
}
