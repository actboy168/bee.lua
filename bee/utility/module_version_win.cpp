#include <bee/utility/module_version_win.h>
#include <bee/format.h>
#include <vector>
#include <assert.h>

namespace bee {
	module_version::module_version()
		: fixed_file_info_(nullptr)
		, translation_size_(0)
		, current_(0)
		, translation_()
		, version_info_()
        , vaild_(false)
	{ }

	module_version::module_version(const wchar_t* module_path)
		: fixed_file_info_(nullptr)
		, translation_size_(0)
		, current_(0)
		, translation_()
		, version_info_()
        , vaild_(create(module_path))
	{ }

	const wchar_t* module_version::operator[] (const wchar_t* key) const {
		if (!vaild_) return L"";
		const wchar_t* value = nullptr;
		if (get_value(translation_[current_].language, translation_[current_].code_page, key, &value)) {
			return value;
		}
		return L"";
	}

	VS_FIXEDFILEINFO* module_version::fixed_file_info() const {
		return fixed_file_info_;
	}

	bool module_version::select_language(WORD langid) {
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

	bool module_version::create(const wchar_t* module_path) {
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

	bool module_version::get_value(WORD language, WORD code_page, const wchar_t* key, const wchar_t** value_ptr) const {
		assert(value_ptr);
		UINT size;
		std::wstring query = std::format(L"\\StringFileInfo\\{:04x}{:04x}\\{}", language, code_page, key);
		return (!!::VerQueryValueW(version_info_.get(), (LPWSTR)(LPCWSTR)query.c_str(), (LPVOID*)value_ptr, &size));
	}

	namespace {
		int stoi_no_throw(std::wstring_view const& str) {
			try {
				return std::stoi(std::wstring(str.data(), str.size()));
			}
			catch (...) {
			}
			return 0;
		}
		template <typename ResultT, typename StrT>
		void split(ResultT& Result, StrT& input, typename StrT::value_type c) {
			for (size_t pos = 0;;) {
				size_t next = input.find(c, pos);
				if (next != StrT::npos) {
					Result.push_back(input.substr(pos, next - pos));
					pos = next + 1;
				}
				else {
					Result.push_back(input.substr(pos));
					break;
				}
			}
		}
		void create_simple_file_version(simple_module_version& sfv, const std::wstring_view& version_string, const wchar_t pred) {
			std::vector<std::wstring_view> version_array;
			split(version_array, version_string, pred);
			sfv.major = (version_array.size() > 0) ? stoi_no_throw(version_array[0]) : 0;
			sfv.minor = (version_array.size() > 1) ? stoi_no_throw(version_array[1]) : 0;
			sfv.revision = (version_array.size() > 2) ? stoi_no_throw(version_array[2]) : 0;
			sfv.build = (version_array.size() > 3) ? stoi_no_throw(version_array[3]) : 0;
		}
	}

	simple_module_version::simple_module_version()
		: major(0)
		, minor(0)
		, revision(0)
		, build(0)
	{ }

	simple_module_version::simple_module_version(const wchar_t* module_path, const wchar_t* key, const wchar_t pred) {
		create_simple_file_version(*this, module_version(module_path)[key], pred);
	}
}
