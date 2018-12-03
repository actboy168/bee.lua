#pragma once

#include <system_error>

namespace bee {
    int last_syserror();
    int last_neterror();
    std::system_error make_syserror(const char* message = nullptr);
    std::system_error make_neterror(const char* message = nullptr);
    std::system_error make_error(int err, const char* message = nullptr);
}
