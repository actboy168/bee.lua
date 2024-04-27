#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <assert.h>
#include <bee/utility/assume.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WTF8_ASSUME(cond) \
    assert(cond);         \
    BEE_ASSUME(cond)

inline uint8_t wtf8_decode(const char* input, uint32_t* res) {
    uint8_t b1 = input[0];
    if (b1 <= 0x7F) {
        *res = b1;
        return 1;
    }
    if (b1 < 0xC2) {
        return 0;
    }
    uint32_t code_point = b1;
    uint8_t b2          = input[1];
    if ((b2 & 0xC0) != 0x80) {
        return 0;
    }
    code_point = (code_point << 6) | (b2 & 0x3F);
    if (b1 <= 0xDF) {
        *res = 0x7FF & code_point;
        return 2;
    }

    uint8_t b3 = input[2];
    if ((b3 & 0xC0) != 0x80) {
        return 0;
    }
    code_point = (code_point << 6) | (b3 & 0x3F);
    if (b1 <= 0xEF) {
        *res = 0xFFFF & code_point;
        return 3;
    }

    uint8_t b4 = input[3];
    if ((b4 & 0xC0) != 0x80) {
        return 0;
    }
    code_point = (code_point << 6) | (b4 & 0x3F);
    if (b1 <= 0xF4) {
        code_point &= 0x1FFFFF;
        if (code_point <= 0x10FFFF) {
            *res = code_point;
            return 4;
        }
    }
    return 0;
}

inline size_t wtf8_to_utf16_length(const char* input, size_t length) {
    size_t output_len = 0;
    uint32_t code_point;
    for (size_t i = 0; i < length;) {
        uint8_t n = wtf8_decode(&input[i], &code_point);
        if (n == 0) {
            return (size_t)-1;
        }
        if (code_point > 0xFFFF) {
            output_len += 2;
        } else {
            output_len += 1;
        }
        i += n;
    }
    return output_len;
}

inline void wtf8_to_utf16(const char* input, size_t length, wchar_t* output, size_t output_len) {
    uint32_t code_point;
    for (size_t i = 0; i < length;) {
        uint8_t n = wtf8_decode(&input[i], &code_point);
        WTF8_ASSUME(n > 0);
        if (code_point > 0x10000) {
            WTF8_ASSUME(code_point < 0x10FFFF);
            *output++ = (((code_point - 0x10000) >> 10) + 0xD800);
            *output++ = ((code_point - 0x10000) & 0x3FF) + 0xDC00;
            output_len -= 2;
        } else {
            *output++ = code_point;
            output_len -= 1;
        }
        i += n;
    }
    (void)output_len;
    WTF8_ASSUME(output_len == 0);
}

inline uint32_t wtf8_surrogate(const wchar_t* input, bool eof) {
    uint32_t u = input[0];
    if (u >= 0xD800 && u <= 0xDBFF && !eof) {
        uint32_t next = input[1];
        if (next >= 0xDC00 && next <= 0xDFFF) {
            return 0x10000 + ((u - 0xD800) << 10) + (next - 0xDC00);
        }
    }
    return u;
}

inline size_t wtf8_from_utf16_length(const wchar_t* input, size_t length) {
    size_t output_len = 0;
    for (size_t i = 0; i < length; ++i) {
        uint32_t code_point = wtf8_surrogate(&input[i], length == i + 1);
        if (code_point == 0) {
            break;
        }
        if (code_point < 0x80) {
            output_len += 1;
        } else if (code_point < 0x800) {
            output_len += 2;
        } else if (code_point < 0x10000) {
            output_len += 3;
        } else {
            output_len += 4;
            i++;
        }
    }
    return output_len;
}

inline void wtf8_from_utf16(const wchar_t* input, size_t length, char* output, size_t output_len) {
    for (size_t i = 0; i < length; ++i) {
        uint32_t code_point = wtf8_surrogate(&input[i], length == i + 1);
        if (code_point == 0) {
            break;
        }
        if (code_point < 0x80) {
            *output++ = code_point;
            output_len -= 1;
        } else if (code_point < 0x800) {
            *output++ = 0xC0 | (code_point >> 6);
            *output++ = 0x80 | (code_point & 0x3F);
            output_len -= 2;
        } else if (code_point < 0x10000) {
            *output++ = 0xE0 | (code_point >> 12);
            *output++ = 0x80 | ((code_point >> 6) & 0x3F);
            *output++ = 0x80 | (code_point & 0x3F);
            output_len -= 3;
        } else {
            *output++ = 0xF0 | (code_point >> 18);
            *output++ = 0x80 | ((code_point >> 12) & 0x3F);
            *output++ = 0x80 | ((code_point >> 6) & 0x3F);
            *output++ = 0x80 | (code_point & 0x3F);
            output_len -= 4;
            i++;
        }
    }
    (void)output_len;
    WTF8_ASSUME(output_len == 0);
}

#if defined(__cplusplus)
}
#endif
