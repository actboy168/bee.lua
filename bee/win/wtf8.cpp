#include <bee/win/cwtf8.h>
#include <bee/win/wtf8.h>

namespace bee::wtf8 {
    std::wstring u2w(zstring_view str) noexcept {
        if (str.empty()) {
            return L"";
        }
        size_t wlen = wtf8_to_utf16_length(str.data(), str.size());
        if (wlen == (size_t)-1) {
            return L"";
        }
        std::wstring wresult(wlen, L'\0');
        wtf8_to_utf16(str.data(), str.size(), wresult.data(), wlen);
        return wresult;
    }

    std::string w2u(wzstring_view wstr) noexcept {
        if (wstr.empty()) {
            return "";
        }
        size_t len = wtf8_from_utf16_length(wstr.data(), wstr.size());
        std::string result(len, '\0');
        wtf8_from_utf16(wstr.data(), wstr.size(), result.data(), len);
        return result;
    }
}
