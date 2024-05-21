#include <bee/crash/nanoid.h>

#include <cmath>
#include <cstdint>
#include <random>
#include <string_view>
#include <vector>

namespace bee {
    static int nanoid_clz32(int x) {
        const int numIntBits = sizeof(int) * 8;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x -= x >> 1 & 0x55555555;
        x = (x >> 2 & 0x33333333) + (x & 0x33333333);
        x = (x >> 4) + (x & 0x0f0f0f0f);
        x += x >> 8;
        x += x >> 16;
        return numIntBits - (x & 0x0000003f);
    }

    template <class Random>
    static void nanoid_next_bytes(Random& random, uint8_t* buffer, size_t size) {
        size_t reps = (size / sizeof(uint32_t)) * sizeof(uint32_t);
        size_t i    = 0;
        for (; i < reps; i += sizeof(uint32_t)) {
            *(uint32_t*)(buffer + i) = random();
        }
        if (i == size) return;
        uint32_t last = random();
        for (; i < size; ++i) {
            *(buffer + i) = (uint8_t)((last >> (8 * (i - reps))));
        }
    }

    template <typename Random, typename CharT>
    static std::basic_string<CharT> nanoid_generate(Random& random, std::basic_string_view<CharT> alphabet, size_t size) {
        size_t alphSize = alphabet.size();
        size_t mask     = (2 << (31 - nanoid_clz32((int)((alphSize - 1) | 1)))) - 1;
        auto step       = (size_t)std::ceil(1.6 * (double)mask * (double)size / (double)alphSize);
        auto idBuilder  = std::basic_string<CharT>(size, (CharT)'_');
        auto bytes      = std::vector<uint8_t>(step);
        size_t cnt      = 0;
        for (;;) {
            nanoid_next_bytes(random, bytes.data(), bytes.size());
            for (size_t i = 0; i < step; ++i) {
                size_t alphabetIndex = bytes[i] & mask;
                if (alphabetIndex >= alphSize) {
                    continue;
                }
                idBuilder[cnt] = alphabet[alphabetIndex];
                if (++cnt == size) {
                    return { idBuilder };
                }
            }
        }
    }

    std::string nanoid() {
        static std::mt19937 random(std::random_device {}());
        constexpr std::string_view dict = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        constexpr size_t size           = 21;
        return nanoid_generate(random, dict, size);
    }
    std::wstring wnanoid() {
        static std::mt19937 random(std::random_device {}());
        constexpr std::wstring_view dict = L"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        constexpr size_t size            = 21;
        return nanoid_generate(random, dict, size);
    }
}
