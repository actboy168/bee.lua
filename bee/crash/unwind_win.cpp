#if defined(_M_IX86)
// clang-format off
#    include <windows.h>
#    include <DbgHelp.h>
// clang-format on
#endif

#include <bee/crash/unwind_win.h>

namespace bee::crash {
    void unwind(const CONTEXT* ctx, uint16_t skip, unwind_callback func, void* ud) noexcept {
#if defined(_M_X64) || defined(_M_ARM64)
#    if defined(_M_X64)
#        define CONTEXT_REG_PC Rip
#    elif defined(_M_ARM64)
#        define CONTEXT_REG_PC Pc
#    endif
        CONTEXT Context;
        UNWIND_HISTORY_TABLE UnwindHistoryTable;
        PRUNTIME_FUNCTION RuntimeFunction;
        PVOID HandlerData;
        ULONG64 EstablisherFrame;
        ULONG64 ImageBase;
        memcpy(&Context, ctx, sizeof(CONTEXT));
        RtlZeroMemory(&UnwindHistoryTable, sizeof(UNWIND_HISTORY_TABLE));
        for (;;) {
            RuntimeFunction = RtlLookupFunctionEntry(Context.CONTEXT_REG_PC, &ImageBase, &UnwindHistoryTable);
            if (!RuntimeFunction) {
#    if defined(_M_X64)
                Context.Rip = (ULONG64)(*(PULONG64)Context.Rsp);
                Context.Rsp += sizeof(ULONG64);
#    elif defined(_M_ARM64)
                Context.Pc = Context.Lr;
#    endif
                break;
            } else {
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, ImageBase, Context.CONTEXT_REG_PC, RuntimeFunction, &Context, &HandlerData, &EstablisherFrame, NULL);
            }
            if (!Context.CONTEXT_REG_PC) {
                break;
            }
            if (skip > 0) {
                skip--;
                continue;
            }
            if (!func((void*)Context.CONTEXT_REG_PC, ud)) {
                break;
            }
        }
#elif defined(_M_IX86)
        CONTEXT Context;
        memcpy(&Context, ctx, sizeof(CONTEXT));
        STACKFRAME64 s;
        memset(&s, 0, sizeof(s));
        s.AddrPC.Offset    = Context.Eip;
        s.AddrPC.Mode      = AddrModeFlat;
        s.AddrFrame.Offset = Context.Ebp;
        s.AddrFrame.Mode   = AddrModeFlat;
        s.AddrStack.Offset = Context.Esp;
        s.AddrStack.Mode   = AddrModeFlat;
        for (;;) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &s, (PVOID)&Context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
                break;
            }
            if (!s.AddrReturn.Offset) {
                break;
            }
            if (skip > 0) {
                skip--;
                continue;
            }
            if (!func((void*)s.AddrReturn.Offset, ud)) {
                break;
            }
        }
#else
#    error "Platform not supported!"
#endif
    }
}
