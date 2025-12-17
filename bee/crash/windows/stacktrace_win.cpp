// clang-format off
#include <initguid.h>
// clang-format on

#include <DbgEng.h>
#include <DbgHelp.h>
#include <bee/crash/stacktrace.h>
#include <bee/crash/strbuilder.h>
#include <bee/utility/span.h>

#include <cstdint>
#include <deque>
#include <functional>

namespace bee::crash {
    class dbg_eng_data {
    public:
        dbg_eng_data() noexcept {
            AcquireSRWLockExclusive(&srw);
        }
        ~dbg_eng_data() noexcept {
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

        void get_description(strbuilder& sb, const void* const address) noexcept {
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

        void source_file(strbuilder& sb, const void* const address, ULONG* const line) noexcept {
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

        void address_to_string(strbuilder& sb, const void* const address) noexcept {
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

    struct stacktrace_impl {
        dbg_eng_data locked_data;
        strbuilder sb;
        unsigned int i = 0;

        bool initialize() noexcept {
            return locked_data.try_initialize();
        }
        void add_frame(void* pc) noexcept {
            constexpr size_t max_entry_num = sizeof("65536> ") - 1;
            sb.expansion(max_entry_num);
            const int ret = std::snprintf(sb.remaining_data(), max_entry_num + 1, "%u> ", i++);
            if (ret <= 0) {
                std::abort();
            }
            sb.append(ret);
            locked_data.address_to_string(sb, pc);
            sb += L"\n";
        }
        std::string to_string() noexcept {
            return sb.to_string();
        }
    };

    stacktrace::stacktrace() noexcept
        : impl(new stacktrace_impl) {}

    stacktrace::~stacktrace() noexcept {
        delete impl;
    }

    bool stacktrace::initialize() noexcept {
        return impl->initialize();
    }
    void stacktrace::add_frame(void* pc) noexcept {
        impl->add_frame(pc);
    }
    std::string stacktrace::to_string() noexcept {
        return impl->to_string();
    }
}
