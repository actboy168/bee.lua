#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_WIN32)

#include "utf8_crt.h"

#include <Windows.h>
#include <assert.h>
#include <bee/platform/win/wtf8_c.h>
#include <io.h>
#include <malloc.h>

#if defined(__GNUC__)
#    define thread_local __thread
#elif defined(_MSC_VER)
#    define thread_local __declspec(thread)
#else
#    error Cannot define thread_local
#endif

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

wchar_t* u2w(const char* str) {
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

char* w2u(const wchar_t* wstr) {
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
    wchar_t* wfilename = u2w(filename);
    wchar_t* wmode     = u2w(mode);
    FILE* ret          = _wfopen(wfilename, wmode);
    free(wfilename);
    free(wmode);
    return ret;
}

FILE* __cdecl utf8_freopen(const char* filename, const char* mode, FILE* stream) {
    wchar_t* wfilename = u2w(filename);
    wchar_t* wmode     = u2w(mode);
    FILE* ret          = _wfreopen(wfilename, wmode, stream);
    free(wfilename);
    free(wmode);
    return ret;
}

FILE* __cdecl utf8_popen(const char* command, const char* type) {
    wchar_t* wcommand = u2w(command);
    wchar_t* wtype    = u2w(type);
    FILE* ret         = _wpopen(wcommand, wtype);
    free(wcommand);
    free(wtype);
    return ret;
}

int __cdecl utf8_system(const char* command) {
    wchar_t* wcommand = u2w(command);
    int ret           = _wsystem(wcommand);
    free(wcommand);
    return ret;
}

int __cdecl utf8_remove(const char* filename) {
    wchar_t* wfilename = u2w(filename);
    int ret            = _wremove(wfilename);
    free(wfilename);
    return ret;
}

int __cdecl utf8_rename(const char* oldfilename, const char* newfilename) {
    wchar_t* woldfilename = u2w(oldfilename);
    wchar_t* wnewfilename = u2w(newfilename);
    int ret               = _wrename(woldfilename, wnewfilename);
    free(woldfilename);
    free(wnewfilename);
    return ret;
}

char* __cdecl utf8_getenv(const char* varname) {
#if defined(__SANITIZE_ADDRESS__)
    return getenv(varname);
#else
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
#endif
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
    return buffer;
}

void* __stdcall utf8_LoadLibraryExA(const char* filename, void* file, unsigned long flags) {
    wchar_t* wfilename = u2w(filename);
    void* ret          = LoadLibraryExW(wfilename, file, flags);
    free(wfilename);
    return ret;
}

unsigned long __stdcall utf8_GetModuleFileNameA(void* module, char* filename, unsigned long size) {
    wchar_t* tmp = calloc(size, sizeof(wchar_t));
    if (!tmp) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }
    DWORD tmplen = GetModuleFileNameW(module, tmp, size);
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
    wchar_t* tmp = calloc(nSize, sizeof(wchar_t));
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
    return (unsigned long)len;
}

static void ConsoleWrite(FILE* stream, const char* s, int l) {
    HANDLE handle = (HANDLE)_get_osfhandle(_fileno(stream));
    if (FILE_TYPE_CHAR != GetFileType(handle)) {
        fwrite(s, sizeof(char), l, stream);
        return;
    }
    struct u2w_result r = u2w_r(s, l);
    if (!r.wstr) {
        fwrite(s, sizeof(char), l, stream);
        return;
    }
    if (!WriteConsoleW(handle, r.wstr, (DWORD)r.wlen, NULL, NULL)) {
        free(r.wstr);
        fwrite(s, sizeof(char), l, stream);
        return;
    }
    free(r.wstr);
}

void utf8_ConsoleWrite(const char* s, int l) {
    ConsoleWrite(stdout, s, l);
}
void utf8_ConsoleNewLine() {
    fwrite("\n", sizeof(char), 1, stdout);
    fflush(stdout);
}
void utf8_ConsoleError(const char* fmt, const char* param) {
    int l   = snprintf(NULL, 0, fmt, param);
    char* s = (char*)calloc(l, sizeof(char));
    if (!s) {
        return;
    }
    snprintf(s, l, fmt, param);
    ConsoleWrite(stderr, s, l);
    fflush(stderr);
    free(s);
}

#endif
