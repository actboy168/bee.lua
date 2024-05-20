#pragma once

#include <windows.h>

#include <string>

namespace bee::crash {
    std::string stacktrace(const CONTEXT* ctx) noexcept;
}
