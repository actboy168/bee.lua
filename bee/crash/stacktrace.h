#pragma once

#include <string>

namespace bee::crash {
    struct stacktrace_impl;
    struct stacktrace {
        stacktrace() noexcept;
        ~stacktrace() noexcept;
        bool initialize() noexcept;
        void add_frame(void* pc) noexcept;
        std::string to_string() noexcept;
        stacktrace_impl* impl;
    };
}
