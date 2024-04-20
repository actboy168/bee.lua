#pragma once

#include <string>
#include <string_view>
#include <system_error>

namespace bee::error {
    std::string errmsg(std::string_view msg, std::error_code ec) noexcept;
    std::string crt_errmsg(std::string_view msg, std::errc err) noexcept;
    std::string crt_errmsg(std::string_view msg) noexcept;
    std::string sys_errmsg(std::string_view msg) noexcept;
    std::string net_errmsg(std::string_view msg, int err) noexcept;
    std::string net_errmsg(std::string_view msg) noexcept;
}
