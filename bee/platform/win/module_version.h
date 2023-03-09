#pragma once

#include <string>

namespace bee::win {
	std::wstring get_module_version(const wchar_t* module_path, const wchar_t* key);
}
