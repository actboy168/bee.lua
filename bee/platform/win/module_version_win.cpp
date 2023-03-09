#include <bee/platform/win/module_version.h>
#include <bee/nonstd/format.h>
#include <vector>
#include <memory>
#include <Windows.h>

namespace bee::win {
	class module_version_info {
	public:
		module_version_info();
		module_version_info(const wchar_t* module_path);
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
		std::unique_ptr<TRANSLATION[]> translation_;
		std::unique_ptr<uint8_t[]> version_info_;
		bool valid_;
	};

	module_version_info::module_version_info()
		: fixed_file_info_(nullptr)
		, translation_size_(0)
		, current_(0)
		, translation_()
		, version_info_()
		, valid_(false)
	{ }

	module_version_info::module_version_info(const wchar_t* module_path)
		: fixed_file_info_(nullptr)
		, translation_size_(0)
		, current_(0)
		, translation_()
		, version_info_()
		, valid_(create(module_path))
	{ }

	const wchar_t* module_version_info::operator[] (const wchar_t* key) const {
		if (!valid_) return L"";
		const wchar_t* value = nullptr;
		if (get_value(translation_[current_].language, translation_[current_].code_page, key, &value)) {
			return value;
		}
		return L"";
	}

	VS_FIXEDFILEINFO* module_version_info::fixed_file_info() const {
		return fixed_file_info_;
	}

	bool module_version_info::select_language(WORD langid) {
		for (size_t i = 0; i < translation_size_; ++i) {
			if (translation_[i].language == langid) {
				current_ = i;
				return true;
			}
		}
		for (size_t i = 0; i < translation_size_; ++i) {
			if (translation_[i].language == 0) {
				current_ = i;
				return true;
			}
		}
		return false;
	}

	bool module_version_info::create(const wchar_t* module_path) {
		DWORD dummy_handle = 0;
		DWORD size = ::GetFileVersionInfoSizeW(module_path, &dummy_handle);
		if (size <= 0) {
			return false;
		}
		version_info_.reset(new uint8_t[size]);
		if (!::GetFileVersionInfoW(module_path, 0, size, version_info_.get())) {
			return false;
		}
		UINT length;
		if (!::VerQueryValueW(version_info_.get(), L"\\", (LPVOID*)&fixed_file_info_, &length)) {
			return false;
		}
		if (fixed_file_info()->dwSignature != VS_FFI_SIGNATURE) {
			return false;
		}
		TRANSLATION* translate_ptr = nullptr;
		if (!::VerQueryValueW(version_info_.get(), L"\\VarFileInfo\\Translation", (LPVOID*)&translate_ptr, &length)
			|| (length < sizeof(TRANSLATION))) {
			return false;
		}
		current_ = 0;
		translation_size_ = length / sizeof(TRANSLATION);
		translation_.reset(new TRANSLATION[translation_size_]);
		memcpy(translation_.get(), translate_ptr, translation_size_ * sizeof(TRANSLATION));
		select_language(::GetUserDefaultLangID());
		return true;
	}

	bool module_version_info::get_value(WORD language, WORD code_page, const wchar_t* key, const wchar_t** value_ptr) const {
		UINT size;
		std::wstring query = std::format(L"\\StringFileInfo\\{:04x}{:04x}\\{}", language, code_page, key);
		return (!!::VerQueryValueW(version_info_.get(), (LPWSTR)(LPCWSTR)query.c_str(), (LPVOID*)value_ptr, &size));
	}

	std::wstring get_module_version(const wchar_t* module_path, const wchar_t* key) {
		module_version_info info(module_path);
		return info[key];
	}
}
