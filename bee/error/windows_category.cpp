#include <bee/error/windows_category.h>
#include <bee/utility/unicode.h>
#include <sstream>
#include <Windows.h>

namespace bee {
    template <class CharT>
    class basic_errormsg {
    public:
        basic_errormsg() : data(nullptr) { }
        basic_errormsg(size_t n) : data(static_cast<CharT*>(::LocalAlloc(0, n * sizeof(CharT)))) { }
        basic_errormsg(const basic_errormsg& that) : data(nullptr) { *this = that; }
        ~basic_errormsg() { reset(0, 0); }
        basic_errormsg& operator =(const basic_errormsg& o) {
            if (this != &o) {
                reset(o.data, ::LocalSize(o.data) / sizeof(CharT));
            }
            return *this;
        }
        void reset(const CharT* buf, size_t len) {
            if (!buf || !len) {
                if (data) {
                    ::LocalFree(reinterpret_cast<HLOCAL>(data));
                }
                data = nullptr;
                return;
            }
            data = static_cast<CharT*>(::LocalAlloc(0, len * sizeof(CharT)));
            if (data) {
                memcpy(data, buf, len * sizeof(CharT));
            }
        }

        CharT*       c_str() { return data; }
        const CharT* c_str() const { return data; }
        CharT**      get() { return &data; }
        operator bool()      const { return !!data; }

    private:
        CharT* data;
    };
    typedef basic_errormsg<char>    errormsg;
    typedef basic_errormsg<wchar_t> werrormsg;

    static std::wstring error_message(int error_code) {
        werrormsg buffer;
        unsigned long result = ::FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(buffer.get()),
            0,
            NULL);

        if ((result == 0) || (!buffer)) {
            std::wostringstream os;
            os << L"Unable to get an error message for error code: " << error_code << ".";
            return os.str();
        }
        std::wstring_view str(buffer.c_str());
        while (str.size() && ((str.back() == L'\n') || (str.back() == L'\r'))) {
            str.remove_suffix(1);
        }
        return std::wstring(str);
    }

    class winCategory : public std::error_category {
    public:
        virtual const char* name() const noexcept {
            return "Windows";
        }
        virtual std::string message(int error_code) const noexcept {
            return std::move(w2u(error_message(error_code)));
        }
        virtual std::error_condition default_error_condition(int error_code) const noexcept {
            const std::error_condition cond = std::system_category().default_error_condition(error_code);
            if (cond.category() == std::generic_category()) {
                return cond;
            }
            return std::error_condition(error_code, *this);
        }
    };

    static winCategory g_windows_category;

    const std::error_category& windows_category() noexcept {
        return g_windows_category;
    }
}
