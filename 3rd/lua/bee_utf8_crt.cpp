#if defined(_WIN32)
#    ifndef _CRT_SECURE_NO_WARNINGS
#        define _CRT_SECURE_NO_WARNINGS
#    endif

#    include "bee_utf8_crt.h"

#    include <Windows.h>
#    include <assert.h>
#    include <bee/platform/win/cwtf8.h>
#    include <bee/platform/win/wtf8.h>
#    include <io.h>
#    include <malloc.h>

#    if defined(__GNUC__)
#        define thread_local __thread
#    elif defined(_MSC_VER)
#        define thread_local __declspec(thread)
#    else
#        error Cannot define thread_local
#    endif

struct u2w_result {
    wchar_t* wstr;
    size_t wlen;
};

static struct u2w_result u2w_r(const char* str, size_t len) {
    struct u2w_result res = { NULL, 0 };
    if (!str) {
        return res;
    }
    size_t wlen = wtf8_to_utf16_length(str, len);
    if (wlen == (size_t)-1) {
        return res;
    }
    res.wstr = (wchar_t*)calloc(wlen + 1, sizeof(wchar_t));
    if (!res.wstr) {
        return res;
    }
    res.wlen = wlen;
    wtf8_to_utf16(str, len, res.wstr, res.wlen);
    return res;
}

static wchar_t* u2w(const char* str) {
    if (!str) {
        return NULL;
    }
    size_t len  = strlen(str);  // TODO
    size_t wlen = wtf8_to_utf16_length(str, len);
    if (wlen == (size_t)-1) {
        return NULL;
    }
    wchar_t* wresult = (wchar_t*)calloc(wlen + 1, sizeof(wchar_t));
    if (!wresult) {
        return NULL;
    }
    wtf8_to_utf16(str, len, wresult, wlen);
    return wresult;
}

static char* w2u(const wchar_t* wstr) {
    if (!wstr) {
        return NULL;
    }
    size_t wlen  = wcslen(wstr);  // TODO
    size_t len   = wtf8_from_utf16_length(wstr, wlen);
    char* result = (char*)calloc(len + 1, sizeof(char));
    if (!result) {
        return NULL;
    }
    wtf8_from_utf16(wstr, wlen, result, len);
    return result;
}

FILE* __cdecl utf8_fopen(const char* filename, const char* mode) {
    auto wfilename = bee::wtf8::u2w(filename);
    auto wmode     = bee::wtf8::u2w(mode);
    return _wfopen(wfilename.c_str(), wmode.c_str());
}

FILE* __cdecl utf8_freopen(const char* filename, const char* mode, FILE* stream) {
    auto wfilename = bee::wtf8::u2w(filename);
    auto wmode     = bee::wtf8::u2w(mode);
    return _wfreopen(wfilename.c_str(), wmode.c_str(), stream);
}

FILE* __cdecl utf8_popen(const char* command, const char* type) {
    auto wcommand = bee::wtf8::u2w(command);
    auto wtype    = bee::wtf8::u2w(type);
    return _wpopen(wcommand.c_str(), wtype.c_str());
}

int __cdecl utf8_system(const char* command) {
    auto wcommand = bee::wtf8::u2w(command);
    return _wsystem(wcommand.c_str());
}

int __cdecl utf8_remove(const char* filename) {
    auto wfilename = bee::wtf8::u2w(filename);
    return _wremove(wfilename.c_str());
}

int __cdecl utf8_rename(const char* oldfilename, const char* newfilename) {
    auto woldfilename = bee::wtf8::u2w(oldfilename);
    auto wnewfilename = bee::wtf8::u2w(newfilename);
    return _wrename(woldfilename.c_str(), wnewfilename.c_str());
}

char* __cdecl utf8_getenv(const char* varname) {
#    if defined(__SANITIZE_ADDRESS__)
    return getenv(varname);
#    else
    wchar_t* wvarname = u2w(varname);
    wchar_t* wret     = _wgetenv(wvarname);
    free(wvarname);
    if (!wret) {
        return NULL;
    }
    static thread_local char* ret = NULL;
    if (ret) {
        free(ret);
    }
    ret = w2u(wret);
    return ret;
#    endif
}

char* __cdecl utf8_tmpnam(char* buffer) {
    assert(buffer);
    wchar_t tmp[L_tmpnam];
    if (!_wtmpnam(tmp)) {
        return NULL;
    }
    size_t wlen = wcslen(tmp);
    size_t len  = wtf8_from_utf16_length(tmp, wlen);
    wtf8_from_utf16(tmp, wlen, buffer, len);
    buffer[len] = '\0';
    return buffer;
}

void* __stdcall utf8_LoadLibraryExA(const char* filename, void* file, unsigned long flags) {
    auto wfilename = bee::wtf8::u2w(filename);
    return LoadLibraryExW(wfilename.c_str(), file, flags);
}

unsigned long __stdcall utf8_GetModuleFileNameA(void* module, char* filename, unsigned long size) {
    wchar_t* tmp = (wchar_t*)calloc(size, sizeof(wchar_t));
    if (!tmp) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }
    DWORD tmplen = GetModuleFileNameW((HMODULE)module, tmp, size);
    if (tmplen == 0) {
        free(tmp);
        return 0;
    }
    size_t len = wtf8_from_utf16_length(tmp, tmplen);
    if (len > size) {
        free(tmp);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }
    wtf8_from_utf16(tmp, tmplen, filename, len);
    free(tmp);
    filename[len] = '\0';
    return (unsigned long)len;
}

unsigned long __stdcall utf8_FormatMessageA(
    unsigned long dwFlags,
    const void* lpSource,
    unsigned long dwMessageId,
    unsigned long dwLanguageId,
    char* lpBuffer,
    unsigned long nSize,
    va_list* Arguments
) {
    wchar_t* tmp = (wchar_t*)calloc(nSize, sizeof(wchar_t));
    if (!tmp) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }
    DWORD tmplen = FormatMessageW(dwFlags, lpSource, dwMessageId, dwLanguageId, tmp, nSize, Arguments);
    if (tmplen == 0) {
        free(tmp);
        return 0;
    }
    size_t len = wtf8_from_utf16_length(tmp, tmplen);
    if (len > nSize) {
        free(tmp);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }
    wtf8_from_utf16(tmp, tmplen, lpBuffer, len);
    free(tmp);
    lpBuffer[len] = '\0';
    return (unsigned long)len;
}

static void ConsoleWrite(FILE* stream, const char* str, size_t len) {
    HANDLE handle = (HANDLE)_get_osfhandle(_fileno(stream));
    if (FILE_TYPE_CHAR != GetFileType(handle)) {
        fwrite(str, sizeof(char), len, stream);
        return;
    }
    auto r = bee::wtf8::u2w(bee::zstring_view { str, len });
    if (r.empty()) {
        fwrite(str, sizeof(char), len, stream);
        return;
    }
    if (!WriteConsoleW(handle, r.data(), (DWORD)r.size(), NULL, NULL)) {
        fwrite(str, sizeof(char), len, stream);
        return;
    }
}

void utf8_ConsoleWrite(const char* str, size_t len) {
    ConsoleWrite(stdout, str, len);
}

void utf8_ConsoleNewLine() {
    fwrite("\n", sizeof(char), 1, stdout);
    fflush(stdout);
}

void utf8_ConsoleError(const char* fmt, const char* param) {
    size_t len = (size_t)snprintf(NULL, 0, fmt, param);
    char* str  = new (std::nothrow) char[len];
    if (!str) {
        return;
    }
    snprintf(str, len, fmt, param);
    ConsoleWrite(stderr, str, len);
    fflush(stderr);
    delete[] str;
}

char** utf8_create_args(int argc, wchar_t** wargv) {
    char** argv = new (std::nothrow) char*[argc + 1];
    if (!argv) {
        return NULL;
    }
    for (int i = 0; i < argc; ++i) {
        argv[i] = w2u(wargv[i]);
    }
    argv[argc] = 0;
    return argv;
}

void utf8_free_args(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        free(argv[i]);
    }
    delete[] argv;
}

#endif
