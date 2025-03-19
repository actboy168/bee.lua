#pragma once

#if defined(_WIN32)

#if defined(__cplusplus)
extern "C" {
#endif

#    include <stdio.h>
#    include <stdlib.h>

FILE* __cdecl utf8_fopen(const char* filename, const char* mode);
FILE* __cdecl utf8_freopen(const char* filename, const char* mode, FILE* stream);
FILE* __cdecl utf8_popen(const char* command, const char* type);
int __cdecl utf8_system(const char* command);
int __cdecl utf8_remove(const char* filename);
int __cdecl utf8_rename(const char* oldfilename, const char* newfilename);
char* __cdecl utf8_getenv(const char* varname);
char* __cdecl utf8_tmpnam(char* buffer);
void* __stdcall utf8_LoadLibraryExA(const char* filename, void* file, unsigned long flags);
unsigned long __stdcall utf8_GetModuleFileNameA(void* module, char* filename, unsigned long size);
unsigned long __stdcall utf8_FormatMessageA(
    unsigned long dwFlags,
    const void* lpSource,
    unsigned long dwMessageId,
    unsigned long dwLanguageId,
    char* lpBuffer,
    unsigned long nSize,
    va_list* Arguments
);
void utf8_ConsoleWrite(const char* msg, size_t sz);
void utf8_ConsoleNewLine();
void utf8_ConsoleError(const char* fmt, const char* param);

#endif

#if defined(__cplusplus)
}
#endif
