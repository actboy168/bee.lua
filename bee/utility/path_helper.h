#pragma once

#include <bee/config.h>
#include <filesystem>
#include <Windows.h>

namespace bee { namespace path {
	namespace fs = std::filesystem;
	_BEE_API fs::path module(HMODULE module_handle = NULL);
	_BEE_API bool     equal(fs::path const& lhs, fs::path const& rhs);
}}
