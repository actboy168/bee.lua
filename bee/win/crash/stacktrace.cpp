// clang-format off
#include <initguid.h>
// clang-format on

#include <DbgEng.h>
#include <DbgHelp.h>
#include <bee/utility/span.h>
#include <bee/win/crash/stacktrace.h>

#include <cstdint>
#include <deque>

namespace bee::crash {
    template <class char_t, size_t N = 1024>
    struct basic_strbuilder {
        struct node {
            char_t data[N];
            size_t size;
            node() noexcept
                : data()
                , size(0) {}
            void append(const char_t* str, size_t n) noexcept {
                if (n > remaining_size()) {
                    std::abort();
                }
                memcpy(remaining_data(), str, n * sizeof(char_t));
                size += n;
            }
            void append(size_t n) noexcept {
                if (n > remaining_size()) {
                    std::abort();
                }
                size += n;
            }
            char_t* remaining_data() noexcept {
                return data + size;
            }
            size_t remaining_size() const noexcept {
                return N - size;
            }
        };
        basic_strbuilder() noexcept
            : deque() {
            deque.emplace_back();
        }
        char_t* remaining_data() noexcept {
            return deque.back().remaining_data();
        }
        size_t remaining_size() const noexcept {
            return deque.back().remaining_size();
        }
        void expansion(size_t n) noexcept {
            if (n > remaining_size()) {
                deque.emplace_back();
                if (n > remaining_size()) {
                    std::abort();
                }
            }
        }
        void append(const char_t* str, size_t n) noexcept {
            auto& back = deque.back();
            if (n <= remaining_size()) {
                back.append(str, n);
            } else {
                deque.emplace_back().append(str, n);
            }
        }
        void append(size_t n) noexcept {
            auto& back = deque.back();
            back.append(n);
        }
        template <class T, size_t n>
        basic_strbuilder& operator+=(T (&str)[n]) noexcept {
            append((const char_t*)str, n - 1);
            return *this;
        }
        basic_strbuilder& operator+=(const std::basic_string_view<char_t>& s) noexcept {
            append(s.data(), s.size());
            return *this;
        }
        std::basic_string<char_t> to_string() const noexcept {
            size_t size = 0;
            for (auto& s : deque) {
                size += s.size;
            }
            std::basic_string<char_t> r(size, '\0');
            size_t pos = 0;
            for (auto& s : deque) {
                memcpy(r.data() + pos, s.data, sizeof(char_t) * s.size);
                pos += s.size;
            }
            return r;
        }
        std::deque<node> deque;
    };
    using strbuilder = basic_strbuilder<char>;

    class dbg_eng_data {
    public:
        dbg_eng_data() noexcept {
            AcquireSRWLockExclusive(&srw);
        }
        ~dbg_eng_data() {
            ReleaseSRWLockExclusive(&srw);
        }
        dbg_eng_data(const dbg_eng_data&)            = delete;
        dbg_eng_data& operator=(const dbg_eng_data&) = delete;
        void uninitialize() noexcept {
            if (debug_client != nullptr) {
                if (attached) {
                    debug_client->DetachProcesses();
                    attached = false;
                }
                debug_client->Release();
                debug_client = nullptr;
            }
            if (debug_control != nullptr) {
                debug_control->Release();
                debug_control = nullptr;
            }
            if (debug_symbols != nullptr) {
                debug_symbols->Release();
                debug_symbols = nullptr;
            }
            if (dbgeng != nullptr) {
                FreeLibrary(dbgeng);
                dbgeng = nullptr;
            }
            initialize_attempted = false;
        }

        bool try_initialize() noexcept {
            if (!initialize_attempted) {
                initialize_attempted = true;
                if (std::atexit(lock_and_uninitialize) != 0) {
                    return false;
                }
                dbgeng = LoadLibraryExW(L"dbgeng.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (dbgeng) {
                    const auto debug_create = reinterpret_cast<decltype(&DebugCreate)>(GetProcAddress(dbgeng, "DebugCreate"));
                    if (debug_create && SUCCEEDED(debug_create(IID_IDebugClient, reinterpret_cast<void**>(&debug_client))) && SUCCEEDED(debug_client->QueryInterface(IID_IDebugSymbols, reinterpret_cast<void**>(&debug_symbols))) && SUCCEEDED(debug_client->QueryInterface(IID_IDebugControl, reinterpret_cast<void**>(&debug_control)))) {
                        attached = SUCCEEDED(debug_client->AttachProcess(0, GetCurrentProcessId(), DEBUG_ATTACH_NONINVASIVE | DEBUG_ATTACH_NONINVASIVE_NO_SUSPEND));
                        if (attached) {
                            debug_control->WaitForEvent(0, INFINITE);
                        }
                        debug_symbols->AddSymbolOptions(SYMOPT_CASE_INSENSITIVE | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_AUTO_PUBLICS | SYMOPT_NO_PROMPTS);
                        debug_symbols->RemoveSymbolOptions(SYMOPT_NO_CPP | SYMOPT_LOAD_ANYTHING | SYMOPT_NO_UNQUALIFIED_LOADS | SYMOPT_EXACT_SYMBOLS | SYMOPT_IGNORE_NT_SYMPATH | SYMOPT_PUBLICS_ONLY | SYMOPT_NO_PUBLICS | SYMOPT_NO_IMAGE_SEARCH);
                    }
                }
            }
            return attached;
        }

        static void lock_and_uninitialize() noexcept {
            dbg_eng_data locked_data;
            locked_data.uninitialize();
        }

        void get_description(strbuilder& sb, const void* const address) {
            ULONG new_size       = 0;
            ULONG64 displacement = 0;
            for (;;) {
                HRESULT hr = debug_symbols->GetNameByOffset(reinterpret_cast<uintptr_t>(address), sb.remaining_data(), static_cast<ULONG>(sb.remaining_size()), &new_size, &displacement);
                if (hr == S_OK) {
                    sb.append(new_size - 1);
                    break;
                } else if (hr == S_FALSE) {
                    sb.expansion(new_size);
                } else {
                    break;
                }
            }
            if (displacement != 0) {
                constexpr size_t max_disp_num = sizeof("+0x1122334455667788") - 1;
                sb.expansion(max_disp_num);
                const int ret = std::snprintf(sb.remaining_data(), max_disp_num + 1, "+0x%llX", displacement);
                if (ret <= 0) {
                    std::abort();
                }
                sb.append(ret);
            }
        }

        void source_file(strbuilder& sb, const void* const address, ULONG* const line) {
            ULONG new_size = 0;
            for (;;) {
                HRESULT hr = debug_symbols->GetLineByOffset(reinterpret_cast<uintptr_t>(address), line, sb.remaining_data(), static_cast<ULONG>(sb.remaining_size()), &new_size, nullptr);
                if (hr == S_OK) {
                    sb.append(new_size - 1);
                    break;
                } else if (hr == S_FALSE) {
                    sb.expansion(new_size);
                } else {
                    break;
                }
            }
        }

        void address_to_string(strbuilder& sb, const void* const address) {
            ULONG line = 0;
            source_file(sb, address, &line);
            if (line != 0) {
                constexpr size_t max_line_num = sizeof("(4294967295): ") - 1;
                sb.expansion(max_line_num);
                const int ret = std::snprintf(sb.remaining_data(), max_line_num + 1, "(%u): ", static_cast<unsigned int>(line));
                if (ret <= 0) {
                    std::abort();
                }
                sb.append(ret);
            }
            get_description(sb, address);
        }

    private:
        inline static SRWLOCK srw                  = SRWLOCK_INIT;
        inline static IDebugClient* debug_client   = nullptr;
        inline static IDebugSymbols* debug_symbols = nullptr;
        inline static IDebugControl* debug_control = nullptr;
        inline static bool attached                = false;
        inline static bool initialize_attempted    = false;
        inline static HMODULE dbgeng               = nullptr;
    };

    static size_t stacktrace(const span<void*>& backtrace, uint16_t skip, const CONTEXT* context) {
#if defined(_M_X64)
        CONTEXT Context;
        UNWIND_HISTORY_TABLE UnwindHistoryTable;
        PRUNTIME_FUNCTION RuntimeFunction;
        PVOID HandlerData;
        ULONG64 EstablisherFrame;
        ULONG64 ImageBase;
        size_t Frame = 0;
        memcpy(&Context, context, sizeof(CONTEXT));
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
            if (Frame >= backtrace.size()) {
                break;
            }
            backtrace[Frame++] = (void*)Context.Rip;
        }
        return Frame;
#elif defined(_M_IX86)
        CONTEXT Context;
        memcpy(&Context, context, sizeof(CONTEXT));
        STACKFRAME64 s;
        memset(&s, 0, sizeof(s));
        s.AddrPC.Offset    = Context.Eip;
        s.AddrPC.Mode      = AddrModeFlat;
        s.AddrFrame.Offset = Context.Ebp;
        s.AddrFrame.Mode   = AddrModeFlat;
        s.AddrStack.Offset = Context.Esp;
        s.AddrStack.Mode   = AddrModeFlat;
        size_t Frame       = 0;
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
            if (Frame >= backtrace.size()) {
                break;
            }
            backtrace[Frame++] = (void*)s.AddrReturn.Offset;
        }
        return Frame;
#else
#    error "Platform not supported!"
#endif
    }

    std::string stacktrace(const CONTEXT* context) {
        dbg_eng_data locked_data;
        if (!locked_data.try_initialize()) {
            return {};
        }
        void* frames[4096];
        size_t size = stacktrace(frames, 0, context);
        strbuilder sb;
        for (size_t i = 0; i < size; ++i) {
            constexpr size_t max_entry_num = sizeof("65536> ") - 1;
            sb.expansion(max_entry_num);
            const int ret = std::snprintf(sb.remaining_data(), max_entry_num + 1, "%u> ", static_cast<unsigned int>(i));
            if (ret <= 0) {
                std::abort();
            }
            sb.append(ret);
            locked_data.address_to_string(sb, frames[i]);
            sb.expansion(1);
            sb += L"\n";
        }
        return sb.to_string();
    }
}
