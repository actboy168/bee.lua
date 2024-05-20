#pragma once

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__SANITIZE_ADDRESS__)
#    include <bee/crash/handler_win.h>
#elif defined(__linux__)
#    include <bee/crash/handler_linux.h>
#else
namespace bee::crash {
    class handler {
    public:
        handler(const char* dump_path) noexcept {}
    };
}
#endif
