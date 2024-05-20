#pragma once

#include <sys/ucontext.h>

#include <string>

namespace bee::crash {
    std::string stacktrace(ucontext_t* ctx) noexcept;
}
