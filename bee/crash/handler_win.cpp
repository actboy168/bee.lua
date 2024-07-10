#include <bee/crash/handler_win.h>
#include <bee/crash/nanoid.h>
#include <bee/crash/stacktrace.h>
#include <bee/crash/unwind_win.h>
#include <bee/win/wtf8.h>
#include <io.h>

#include <atomic>
#include <cassert>
#include <cstdio>

namespace bee::crash {
    struct console {
        console() {
            HANDLE h = (HANDLE)_get_osfhandle(_fileno(stdout));
            if (FILE_TYPE_CHAR == GetFileType(h)) {
                handle = h;
            } else {
                handle = NULL;
            }
        }
        template <class char_t, size_t n>
        void print(const char_t (&str)[n]) noexcept {
            if (handle) {
                if constexpr (std::is_same_v<std::remove_cv_t<char_t>, char>) {
                    WriteConsoleA(handle, str, (DWORD)(n - 1), NULL, NULL);
                } else if constexpr (std::is_same_v<std::remove_cv_t<char_t>, wchar_t>) {
                    WriteConsoleW(handle, str, (DWORD)(n - 1), NULL, NULL);
                }
            }
        }
        template <class char_t, size_t n>
        void print(char_t (&str)[n]) noexcept {
            if (handle) {
                if constexpr (std::is_same_v<std::remove_cv_t<char_t>, char>) {
                    WriteConsoleA(handle, str, (DWORD)strlen(str), NULL, NULL);
                } else if constexpr (std::is_same_v<std::remove_cv_t<char_t>, wchar_t>) {
                    WriteConsoleW(handle, str, (DWORD)wcslen(str), NULL, NULL);
                }
            }
        }
        template <class char_t>
        void print(const std::basic_string<char_t>& str) noexcept {
            if (handle) {
                if constexpr (std::is_same_v<std::remove_cv_t<char_t>, char>) {
                    WriteConsoleA(handle, str.data(), (DWORD)str.size(), NULL, NULL);
                } else if constexpr (std::is_same_v<std::remove_cv_t<char_t>, wchar_t>) {
                    WriteConsoleW(handle, str.data(), (DWORD)str.size(), NULL, NULL);
                }
            }
        }
        HANDLE handle;
    };

#ifndef DBG_PRINTEXCEPTION_WIDE_C
#    define DBG_PRINTEXCEPTION_WIDE_C ((DWORD)0x4001000A)
#endif
    static std::atomic<handler*> handler_        = nullptr;
    constexpr int kHandlerThreadInitialStackSize = 64 * 1024;

    handler::handler(const char* dump_path) noexcept {
        handler* expected = nullptr;
        if (handler_.compare_exchange_strong(expected, this)) {
            if (dump_path[0] == '-' && dump_path[1] == '\0') {
                dump_path_[0] = L'\0';
            } else {
                _snwprintf_s(dump_path_, sizeof(dump_path_) / sizeof(dump_path_[0]), _TRUNCATE, L"%s/crash_%s.log", wtf8::u2w(dump_path).c_str(), wnanoid().c_str());
            }
            DWORD thread_id;
            thread_ = CreateThread(NULL, kHandlerThreadInitialStackSize, thread_func, this, 0, &thread_id);
            assert(thread_ != NULL);
            previous_filter_ = SetUnhandledExceptionFilter(handle_exception);
            previous_iph_    = _set_invalid_parameter_handler(handle_invalid_parameter);
            previous_pch_    = _set_purecall_handler(handle_pure_virtual_call);
        }
    }

    handler::~handler() noexcept {
        SetUnhandledExceptionFilter(previous_filter_);
        _set_invalid_parameter_handler(previous_iph_);
        _set_purecall_handler(previous_pch_);
        auto expected = this;
        handler_.compare_exchange_strong(expected, nullptr);
        TerminateThread(thread_, 1);
        CloseHandle(thread_);
        thread_ = NULL;
    }

    DWORD handler::thread_func(void* arg) noexcept {
        handler* self = reinterpret_cast<handler*>(arg);
        assert(self);
        for (;;) {
            self->start_semaphore_.acquire();
            self->return_value_ = self->write_dump(self->req_thread_id_, self->req_thread_, self->exception_info_);
            self->finish_semaphore_.release();
        }
        return 0;
    }

    LONG handler::handle_exception(EXCEPTION_POINTERS* exinfo) noexcept {
        handler* current_handler = handler_.load();
        bool success             = false;
#if defined(_MSC_VER) && !defined(__clang__)
        DWORD code              = exinfo->ExceptionRecord->ExceptionCode;
        bool is_debug_exception = (code == EXCEPTION_BREAKPOINT) || (code == EXCEPTION_SINGLE_STEP) || (code == DBG_PRINTEXCEPTION_C) || (code == DBG_PRINTEXCEPTION_WIDE_C);
#else
        bool is_debug_exception = false;
#endif
        if (!is_debug_exception) {
            success = current_handler->notify_write_dump(exinfo);
        }
        if (success) {
            return EXCEPTION_EXECUTE_HANDLER;
        } else if (current_handler->previous_filter_) {
            return current_handler->previous_filter_(exinfo);
        } else {
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    void handler::handle_invalid_parameter(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t reserved) noexcept {
        handler* current_handler          = handler_.load();
        EXCEPTION_RECORD exception_record = {};
        CONTEXT exception_context         = {};
        EXCEPTION_POINTERS exception_ptrs = { &exception_record, &exception_context };
        ::RtlCaptureContext(&exception_context);
        exception_record.ExceptionCode           = STATUS_INVALID_PARAMETER;
        exception_record.NumberParameters        = 4;
        exception_record.ExceptionInformation[0] = reinterpret_cast<ULONG_PTR>(expression);
        exception_record.ExceptionInformation[1] = reinterpret_cast<ULONG_PTR>(function);
        exception_record.ExceptionInformation[2] = reinterpret_cast<ULONG_PTR>(file);
        exception_record.ExceptionInformation[3] = line;
        if (!current_handler->notify_write_dump(&exception_ptrs)) {
            if (current_handler->previous_iph_) {
                current_handler->previous_iph_(expression, function, file, line, reserved);
            } else {
#if defined(_MSC_VER)
#    if !defined(NDEBUG)
                _invalid_parameter(expression, function, file, line, reserved);
#    else
                _invalid_parameter_noinfo();
#    endif
#endif
            }
        }
        exit(0);
    }

    void handler::handle_pure_virtual_call() noexcept {
        handler* current_handler          = handler_.load();
        EXCEPTION_RECORD exception_record = {};
        CONTEXT exception_context         = {};
        EXCEPTION_POINTERS exception_ptrs = { &exception_record, &exception_context };
        ::RtlCaptureContext(&exception_context);
        exception_record.ExceptionCode    = STATUS_NONCONTINUABLE_EXCEPTION;
        exception_record.NumberParameters = 0;
        if (!current_handler->notify_write_dump(&exception_ptrs)) {
            if (current_handler->previous_pch_) {
                current_handler->previous_pch_();
            } else {
                return;
            }
        }
        exit(0);
    }

    bool handler::notify_write_dump(EXCEPTION_POINTERS* exinfo) noexcept {
        AcquireSRWLockExclusive(&lock_);
        if (thread_ == NULL) {
            ReleaseSRWLockExclusive(&lock_);
            return false;
        }
        req_thread_id_  = GetCurrentThreadId();
        req_thread_     = GetCurrentThread();
        exception_info_ = exinfo;
        start_semaphore_.release();
        finish_semaphore_.acquire();
        bool status     = return_value_;
        req_thread_id_  = 0;
        req_thread_     = NULL;
        exception_info_ = NULL;
        ReleaseSRWLockExclusive(&lock_);
        return status;
    }

    struct writefile {
        writefile(const wchar_t* filename) noexcept
            : file(CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) {}
        ~writefile() noexcept {
            if (file != INVALID_HANDLE_VALUE) {
                CloseHandle(file);
            }
        }
        explicit operator bool() const noexcept {
            return file != INVALID_HANDLE_VALUE;
        }
        bool write(const std::string& str) noexcept {
            return !!WriteFile(file, str.data(), (DWORD)str.size(), NULL, NULL);
        }
        template <size_t n>
        bool write(char (&str)[n]) noexcept {
            return !!WriteFile(file, str, (DWORD)(n - 1), NULL, NULL);
        }
        HANDLE file;
    };

    static std::string get_stacktrace(const CONTEXT* ctx) noexcept {
        stacktrace s;
        if (!s.initialize()) {
            return {};
        }
        constexpr auto func = +[](void* pc, void* ud) {
            stacktrace* s = (stacktrace*)ud;
            s->add_frame(pc);
            return true;
        };
        unwind(ctx, 0, func, &s);
        return s.to_string();
    }

    bool handler::write_dump(DWORD thread_id, HANDLE thread_handle, EXCEPTION_POINTERS* exinfo) noexcept {
        auto str = get_stacktrace(exinfo->ContextRecord);
        if (dump_path_[0] == L'\0') {
            console c;
            c.print(L"\n\nCrash log: \n");
            c.print(str);
            c.print(L"\n");
            return true;
        }
        do {
            writefile file(dump_path_);
            if (!file) {
                break;
            }
            if (!file.write("stacktrace:\n")) {
                break;
            }
            if (!file.write(str)) {
                break;
            }
        } while (false);
        console c;
        c.print(L"\n\nCrash log generated at: ");
        c.print(dump_path_);
        c.print(L"\n");
        return true;
    }
}
