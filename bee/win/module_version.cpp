#include <Windows.h>
#include <bee/nonstd/format.h>
#include <bee/win/module_version.h>

namespace bee::win {

    module_version::module_version(const wchar_t* module_path) noexcept
        : current_(0)
        , translation_()
        , version_info_() {
        DWORD dummy_handle = 0;
        const DWORD size   = ::GetFileVersionInfoSizeW(module_path, &dummy_handle);
        if (size <= 0) {
            return;
        }
        version_info_ = dynarray<std::byte>(size);
        if (!::GetFileVersionInfoW(module_path, 0, size, version_info_.data())) {
            return;
        }
        UINT length;
        VS_FIXEDFILEINFO* fixed_file_info;
        if (!::VerQueryValueW(version_info_.data(), L"\\", (LPVOID*)&fixed_file_info, &length)) {
            return;
        }
        if (fixed_file_info->dwSignature != VS_FFI_SIGNATURE) {
            return;
        }
        translation* translate_ptr = nullptr;
        if (!::VerQueryValueW(version_info_.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&translate_ptr, &length)) {
            return;
        }
        if (length < sizeof(translation)) {
            return;
        }
        translation_ = dynarray<translation>(translate_ptr, length / sizeof(translation));
        select_language(::GetUserDefaultLangID());
    }

    std::wstring_view module_version::get_value(const wchar_t* key) const noexcept {
        if (translation_.empty()) {
            return L"";
        }
        const wchar_t* value = nullptr;
        UINT size;
        std::wstring query = std::format(L"\\StringFileInfo\\{:04x}{:04x}\\{}", translation_[current_].language, translation_[current_].code_page, key);
        if (!::VerQueryValueW(version_info_.data(), (LPWSTR)(LPCWSTR)query.c_str(), (LPVOID*)&value, &size)) {
            return L"";
        }
        return { value, size };
    }

    bool module_version::select_language(uint16_t langid) noexcept {
        for (size_t i = 0; i < translation_.size(); ++i) {
            if (translation_[i].language == langid) {
                current_ = i;
                return true;
            }
        }
        for (size_t i = 0; i < translation_.size(); ++i) {
            if (translation_[i].language == 0) {
                current_ = i;
                return true;
            }
        }
        return false;
    }
}
