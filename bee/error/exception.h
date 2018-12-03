#pragma once

#include  <bee/utility/dynarray.h>

namespace bee {
	class exception : public std::exception {
	public:
		exception();
		exception(const char* fmt, ...);
		exception(const wchar_t* fmt, ...);
		virtual ~exception();
		virtual const char* what() const;
	protected:
		std::dynarray<char> what_;
	};
}
