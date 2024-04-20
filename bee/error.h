#pragma once

#include <string>
#include <string_view>
#include <system_error>

namespace bee::error {
    std::string errmsg(std::string_view msg, std::error_code ec);
    std::string crt_errmsg(std::string_view msg);
    std::string crt_errmsg(std::string_view msg, std::errc err);
    std::string sys_errmsg(std::string_view msg);
    std::string net_errmsg(std::string_view msg);
    std::string net_errmsg(std::string_view msg, int err);
}
