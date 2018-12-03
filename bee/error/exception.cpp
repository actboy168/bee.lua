#include <bee/error/exception.h>
#include <bee/utility/unicode.h>
#include <stdarg.h>

namespace bee {
	exception::exception()
		: what_(0)
	{ }

	exception::exception(const char* fmt, ...)
		: exception()
	{
		va_list va;
		va_start(va, fmt);
		size_t sz = ::_vscprintf(fmt, va);
		std::dynarray<char> str(sz + 1);
		int n = ::_vsnprintf_s(str.data(), sz + 1, sz, fmt, va);
		if (n > 0) {
			what_ = std::move(str);
		}
		va_end(va);
	}

	exception::exception(const wchar_t* fmt, ...)
		: exception()
	{
		va_list va;
		va_start(va, fmt);
		size_t sz = ::_vscwprintf(fmt, va);
		std::dynarray<wchar_t> str(sz + 1);
		int n = ::_vsnwprintf_s(str.data(), sz + 1, sz, fmt, va);
		if (n > 0) {
			std::string ustr = w2u(std::wstring_view(str.data(), n));
			what_ = std::dynarray<char>(ustr.size() + 1);
			memcpy(what_.data(), ustr.data(), ustr.size() + 1);
		}
		va_end(va);
	}

	exception::~exception()
	{ }

	const char* exception::what() const {
		return what_.data() ? what_.data() : "unknown bee::exception";
	}
}
