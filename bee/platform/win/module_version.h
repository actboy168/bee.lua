#pragma once

#include <string>
#include <bee/utility/dynarray.h>
#include <string_view>
#include <Windows.h>

namespace bee::win {
    class module_version {
    public:
        module_version(const wchar_t* module_path);
        bool select_language(WORD langid);
        std::wstring_view get_value(const wchar_t* key) const;

    protected:
        struct TRANSLATION {
            WORD language;
            WORD code_page;
        };
        size_t current_;
        dynarray<TRANSLATION> translation_;
        dynarray<std::byte> version_info_;
    };
}
