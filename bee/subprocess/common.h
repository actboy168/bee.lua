#pragma once

#include <bee/utility/dynarray.h>
#include <bee/utility/file_handle.h>

#include <utility>

namespace bee::subprocess {
    struct environment {
#if defined(_WIN32)
        using value_type = wchar_t;
#else
        using value_type = char*;
#endif
        using ptr_type = dynarray<value_type>;
        ptr_type v;
        environment(std::nullptr_t) noexcept
            : v() {}
        environment(ptr_type&& o) noexcept
            : v(std::move(o)) {}
        environment(environment&& o) noexcept
            : v(std::move(o.v)) {}
        ~environment() {
#if !defined(_WIN32)
            if (!v.empty()) {
                for (char** p = v.data(); *p; ++p) {
                    delete[] (*p);
                }
            }
#endif
        }
        environment(const environment&)            = delete;
        environment& operator=(const environment&) = delete;
        environment& operator=(environment&& o) noexcept {
            std::swap(v, o.v);
            return *this;
        }
        operator bool() const noexcept {
            return !v.empty();
        }
        operator value_type*() noexcept {
            return v.data();
        }
    };

    enum class stdio {
        eInput,
        eOutput,
        eError,
    };

    namespace pipe {
        struct open_result {
            file_handle rd;
            file_handle wr;
            inline FILE* open_read() noexcept {
                return rd.to_file(file_handle::mode::read);
            }
            inline FILE* open_write() noexcept {
                return wr.to_file(file_handle::mode::write);
            }
            operator bool() const noexcept {
                return rd && wr;
            }
        };
        open_result open() noexcept;
        int peek(FILE* f) noexcept;
    }
}
