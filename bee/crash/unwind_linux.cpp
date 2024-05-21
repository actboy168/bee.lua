#include <bee/crash/unwind_linux.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

namespace bee::crash {
    void unwind(ucontext_t *ctx, uint16_t skip, unwind_callback func, void *ud) noexcept {
        unw_cursor_t cursor;
        unw_init_local(&cursor, ctx);
        while (unw_step(&cursor) > 0) {
            unw_word_t pc;
            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            if (pc == 0) {
                break;
            }
            if (!func((void *)pc, ud)) {
                break;
            }
        }
    }
}
