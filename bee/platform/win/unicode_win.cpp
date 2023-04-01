#include <Windows.h>
#include <bee/platform/win/unicode.h>

namespace bee::win {
    std::wstring u2w(zstring_view str) {
        if (str.empty()) {
            return L"";
        }
        const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
        if (wlen <= 0) {
            return L"";
        }
        std::wstring wresult(wlen, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wresult.data(), static_cast<int>(wresult.size()));
        return wresult;
    }

    std::string w2u(wzstring_view wstr) {
        if (wstr.empty()) {
            return "";
        }
        const int len = ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, 0, 0);
        if (len <= 0) {
            return "";
        }
        std::string result(len, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), static_cast<int>(result.size()), 0, 0);
        return result;
    }

    std::wstring a2w(zstring_view str) {
        if (str.empty()) {
            return L"";
        }
        const int wlen = ::MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
        if (wlen <= 0) {
            return L"";
        }
        std::wstring wresult(wlen, L'\0');
        ::MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), wresult.data(), static_cast<int>(wresult.size()));
        return wresult;
    }

    std::string w2a(wzstring_view wstr) {
        if (wstr.empty()) {
            return "";
        }
        const int len = ::WideCharToMultiByte(CP_ACP, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, 0, 0);
        if (len <= 0) {
            return "";
        }
        std::string result(len, '\0');
        ::WideCharToMultiByte(CP_ACP, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), static_cast<int>(result.size()), 0, 0);
        return result;
    }

    std::string a2u(zstring_view str) {
        return w2u(a2w(str));
    }

    std::string u2a(zstring_view str) {
        return w2a(u2w(str));
    }
}
