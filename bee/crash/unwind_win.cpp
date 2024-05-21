#if defined(_M_IX86)
// clang-format off
#    include <windows.h>
#    include <DbgHelp.h>
// clang-format on
#endif

#include <bee/crash/unwind_win.h>

namespace bee::crash {
    void unwind(const CONTEXT* ctx, uint16_t skip, unwind_callback func, void* ud) noexcept {
#if defined(_M_X64)
        CONTEXT Context;
        UNWIND_HISTORY_TABLE UnwindHistoryTable;
        PRUNTIME_FUNCTION RuntimeFunction;
        PVOID HandlerData;
        ULONG64 EstablisherFrame;
        ULONG64 ImageBase;
        memcpy(&Context, ctx, sizeof(CONTEXT));
        RtlZeroMemory(&UnwindHistoryTable, sizeof(UNWIND_HISTORY_TABLE));
        for (;;) {
            RuntimeFunction = RtlLookupFunctionEntry(Context.Rip, &ImageBase, &UnwindHistoryTable);
            if (!RuntimeFunction) {
                Context.Rip = (ULONG64)(*(PULONG64)Context.Rsp);
                Context.Rsp += 8;
            } else {
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, ImageBase, Context.Rip, RuntimeFunction, &Context, &HandlerData, &EstablisherFrame, NULL);
            }
            if (!Context.Rip) {
                break;
            }
            if (skip > 0) {
                skip--;
                continue;
            }
            if (!func((void*)Context.Rip, ud)) {
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
