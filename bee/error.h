#pragma once

#include <string>
#include <string_view>
#include <system_error>

namespace bee {
    const std::error_category& get_error_category() noexcept;
    std::string make_crterror(std::string_view errmsg);
    std::string make_syserror(std::string_view errmsg);
    std::string make_neterror(std::string_view errmsg);
    std::string make_error(std::error_code errcode, std::string_view errmsg);
}
