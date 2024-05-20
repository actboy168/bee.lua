#pragma once

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__SANITIZE_ADDRESS__)
#    include <bee/crash/handler_win.h>
#elif defined(__linux__)
#    include <bee/crash/handler_linux.h>
#else
namespace bee::crash {
    class empty_handler {
    public:
        empty_handler(const char* dump_path) noexcept {}
    };
    using handler = empty_handler;
}
#endif
