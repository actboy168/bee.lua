#pragma once

#include <memory>
#include <utility>
#include <bee/utility/file_handle.h>

namespace bee::subprocess {
    struct environment {
#if defined(_WIN32)
        using value_type = wchar_t;
#else
        using value_type = char*;
#endif
        using ptr_type = std::unique_ptr<value_type[]>;
        ptr_type v;
        environment(std::nullptr_t)
            : v()
        { }
        environment(ptr_type&& o)
            : v(std::move(o))
        { }
        environment(environment&& o)
            : v(std::move(o.v))
        { }
        ~environment() {
#if !defined(_WIN32)
            if (v) {
                for (char** p = v.get(); *p; ++p) {
                    delete[] (*p);
                }
            }
#endif
        }
        environment(const environment&) = delete;
        environment& operator=(const environment&) = delete;
        environment& operator=(environment&& o) {
            std::swap(v, o.v);
            return *this;
        }
        operator bool() const {
            return !!v;
        }
        operator value_type*() const {
            return v.get();
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
            inline FILE* open_read() {
                return rd.to_file(file_handle::mode::read);
            }
            inline FILE* open_write() {
                return wr.to_file(file_handle::mode::write);
            }
            operator bool() const { return rd && wr; }
        };
        open_result open();
        int         peek(FILE* f);
    }
}
