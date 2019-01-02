#pragma once

#include <bee/config.h>
#include <Windows.h>
#include <stdint.h>
#include <memory>

namespace bee {
	class _BEE_API module_version {
		const static WORD ansi_code_page = 1252;
	public:
		module_version();
		module_version(const wchar_t* module_path);
		const wchar_t* operator[] (const wchar_t* key) const;
		VS_FIXEDFILEINFO* fixed_file_info() const;
		bool select_language(WORD langid);

	protected:
		bool create(const wchar_t* module_path);
		bool get_value(WORD language, WORD code_page, const wchar_t* key, const wchar_t** value_ptr) const;

	protected:
		struct TRANSLATION {
			WORD language;
			WORD code_page;
		};

		VS_FIXEDFILEINFO* fixed_file_info_;
		size_t translation_size_;
		size_t current_;
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4251)
#endif
		std::unique_ptr<TRANSLATION[]> translation_;
		std::unique_ptr<uint8_t[]> version_info_;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        bool vaild_;
	};

	struct _BEE_API simple_module_version {
		simple_module_version();
		simple_module_version(const wchar_t* module_path, const wchar_t* key = L"FileVersion", const wchar_t pred = L',');
		uint32_t major;
		uint32_t minor;
		uint32_t revision;
		uint32_t build;
	};
}
