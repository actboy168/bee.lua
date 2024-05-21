#pragma once

#include <windows.h>

#include <cstdint>

namespace bee::crash {
    using unwind_callback = bool (*)(void* pc, void* ud);
    void unwind(const CONTEXT* ctx, uint16_t skip, unwind_callback func, void* ud) noexcept;
}
