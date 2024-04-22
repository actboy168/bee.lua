#pragma once

#include <cassert>

namespace bee {
    template <typename T, size_t N>
    struct hybrid_array {
        T m_data[N];
        T* m_ptr;
        size_t m_size;
        hybrid_array(size_t n) noexcept
            : m_ptr(n <= N ? m_data : new T[n])
            , m_size(n) {
        }
        ~hybrid_array() noexcept {
            if (m_ptr != m_data) {
                delete[] m_ptr;
            }
        }
        const T* data() const noexcept {
            return m_ptr;
        }
        T* data() noexcept {
            return m_ptr;
        }
        size_t size() const noexcept {
            return m_size;
        }
        const T& operator[](size_t n) const noexcept {
            assert(n <= m_size);
            return m_ptr[n];
        }
        T& operator[](size_t n) noexcept {
            assert(n <= m_size);
            return m_ptr[n];
        }
    };
}
