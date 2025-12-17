#pragma once

#include <bee/thread/atomic_semaphore.h>
#include <windows.h>

namespace bee::crash {
    class handler {
    public:
        handler(const char* dump_path) noexcept;
        ~handler() noexcept;
        handler(const handler&)            = delete;
        handler& operator=(const handler&) = delete;

    private:
        static DWORD WINAPI thread_func(void* parameter) noexcept;
        static LONG WINAPI handle_exception(EXCEPTION_POINTERS* exinfo) noexcept;
        static void handle_invalid_parameter(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t reserved) noexcept;
        static void handle_pure_virtual_call() noexcept;
        bool notify_write_dump(EXCEPTION_POINTERS* exinfo) noexcept;
        bool write_dump(DWORD thread_id, HANDLE thread_handle, EXCEPTION_POINTERS* exinfo) noexcept;

        wchar_t dump_path_[1024];
        atomic_semaphore start_semaphore_ { 0 };
        atomic_semaphore finish_semaphore_ { 0 };
        LPTOP_LEVEL_EXCEPTION_FILTER previous_filter_ = NULL;
        _invalid_parameter_handler previous_iph_      = NULL;
        _purecall_handler previous_pch_               = NULL;
        SRWLOCK lock_                                 = SRWLOCK_INIT;
        HANDLE thread_                                = NULL;
        DWORD req_thread_id_                          = 0;
        HANDLE req_thread_                            = NULL;
        EXCEPTION_POINTERS* exception_info_           = NULL;
        bool return_value_                            = false;
    };
}
