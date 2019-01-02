#pragma once

#include <bee/config.h>
#include <system_error>

namespace bee {
    _BEE_API int last_crterror();
    _BEE_API int last_syserror();
    _BEE_API int last_neterror();
    _BEE_API std::system_error make_crterror(const char* message = nullptr);
    _BEE_API std::system_error make_syserror(const char* message = nullptr);
    _BEE_API std::system_error make_neterror(const char* message = nullptr);
    _BEE_API std::system_error make_error(int err, const char* message = nullptr);
}
