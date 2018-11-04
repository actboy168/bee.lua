#pragma once

#include <bee/config.h>
#include <bee/exception/exception.h>
#include <Windows.h>
#include <system_error>

namespace bee {
	class _BEE_API windows_exception : public exception
	{
	public:
		windows_exception(const char* reason = nullptr, int error_code = ::GetLastError());
		const std::error_code& code() const;

	protected:
	#pragma warning(suppress:4251)
		std::error_code error_code_;
	};
}
