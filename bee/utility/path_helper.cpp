#include <bee/utility/path_helper.h>
#include <bee/exception/windows_exception.h>
#include <bee/utility/dynarray.h>
#include <Windows.h>

namespace bee { namespace path {
	fs::path module(HMODULE module_handle) {
		wchar_t buffer[MAX_PATH];
		DWORD path_len = ::GetModuleFileNameW(module_handle, buffer, _countof(buffer));
		if (path_len == 0) {
			throw windows_exception("::GetModuleFileNameW failed.");
		}
		if (path_len < _countof(buffer)) {
			return std::move(fs::path(buffer, buffer + path_len));
		}
		for (size_t buf_len = 0x200; buf_len <= 0x10000; buf_len <<= 1) {
			std::dynarray<wchar_t> buf(path_len);
			path_len = ::GetModuleFileNameW(module_handle, buf.data(), buf.size());
			if (path_len == 0) {
				throw windows_exception("::GetModuleFileNameW failed.");
			}
			if (path_len < _countof(buffer)) {
				return std::move(fs::path(buf.begin(), buf.end()));
			}
		}
		throw std::logic_error("::GetModuleFileNameW return too long.");
	}

	bool equal(fs::path const& lhs, fs::path const& rhs)
	{
		fs::path lpath = fs::absolute(lhs);
		fs::path rpath = fs::absolute(rhs);
		const fs::path::value_type* l(lpath.c_str());
		const fs::path::value_type* r(rpath.c_str());
		while ((towlower(*l) == towlower(*r) || (*l == L'\\' && *r == L'/') || (*l == L'/' && *r == L'\\')) && *l)
		{ 
			++l; ++r; 
		}
		return *l == *r;
	}
}}
