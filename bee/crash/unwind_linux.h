#pragma once

#include <sys/ucontext.h>

#include <cstdint>

namespace bee::crash {
    using unwind_callback = bool (*)(void* pc, void* ud);
    void unwind(ucontext_t* ctx, uint16_t skip, unwind_callback func, void* ud) noexcept;
}
