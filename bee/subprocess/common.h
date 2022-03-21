#pragma once

#include <utility>

namespace bee::subprocess {
    struct environment {
#if defined(_WIN32)
        using value_type = wchar_t*;
#else
        using value_type = char**;
#endif
        value_type v;
        environment(value_type v)
            : v(v)
        { }
        ~environment() {
            if (!v) {
                return;
            }
#if !defined(_WIN32)
            for (char** p = v; *p; ++p) {
                delete[] (*p);
            }
#endif
            delete[] v;
        }
        environment(const environment&) = delete;
        environment& operator=(const environment&) = delete;
        environment(environment&& o)
            : v(o.v) {
            o.v = nullptr;
        }
        environment& operator=(environment&& o) {
            std::swap(v, o.v);
            o.~environment();
            return *this;
        }
        operator bool() const {
            return !!v;
        }
        operator value_type() const {
            return v;
        }
    };
}
