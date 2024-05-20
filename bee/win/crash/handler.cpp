#include <bee/utility/nanoid.h>
#include <bee/win/crash/handler.h>
#include <bee/win/crash/stacktrace.h>

#include <atomic>
#include <cassert>
#include <cstdio>

namespace bee::crash {
#ifndef DBG_PRINTEXCEPTION_WIDE_C
#    define DBG_PRINTEXCEPTION_WIDE_C ((DWORD)0x4001000A)
#endif
    static std::atomic<handler*> handler_        = nullptr;
    constexpr int kHandlerThreadInitialStackSize = 64 * 1024;

    handler::handler(const wchar_t* dump_path) noexcept {
        handler* expected = nullptr;
        if (handler_.compare_exchange_strong(expected, this)) {
            _snwprintf_s(dump_path_, sizeof(dump_path_) / sizeof(dump_path_[0]), _TRUNCATE, L"%s/crash_%s.log", dump_path, wnanoid().c_str());
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

    DWORD handler::thread_func(void* parameter) noexcept {
        handler* self = reinterpret_cast<handler*>(parameter);
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
        DWORD code               = exinfo->ExceptionRecord->ExceptionCode;
        LONG action;
        bool success = false;
        if ((code != EXCEPTION_BREAKPOINT) && (code != EXCEPTION_SINGLE_STEP) && (code != DBG_PRINTEXCEPTION_C) && (code != DBG_PRINTEXCEPTION_WIDE_C)) {
            success = current_handler->notify_write_dump(exinfo);
        }
        if (success) {
            action = EXCEPTION_EXECUTE_HANDLER;
        } else {
            if (current_handler->previous_filter_) {
                action = current_handler->previous_filter_(exinfo);
            } else {
                action = EXCEPTION_CONTINUE_SEARCH;
            }
        }
        return action;
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
#ifdef _DEBUG
                _invalid_parameter(expression, function, file, line, reserved);
#else
                _invalid_parameter_noinfo();
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
        ~writefile() {
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
    bool handler::write_dump(DWORD thread_id, HANDLE thread_handle, EXCEPTION_POINTERS* exinfo) noexcept {
        auto str = stacktrace(exinfo->ContextRecord);
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
        wprintf(L"\n\nCrash log generated at: %s\n", dump_path_);
        return true;
    }
}
