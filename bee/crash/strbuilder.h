#pragma once

#include <cstring>
#include <deque>
#include <string_view>

namespace bee {
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
}
