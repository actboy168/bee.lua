#include <lua.hpp>

#if defined (_WIN32)

#include <Windows.h>

static unsigned long utf8_GetModuleFileNameA(void* module, char* filename, unsigned long size) {
    wchar_t* tmp = (wchar_t*)calloc(size, sizeof(wchar_t));
    unsigned long tmplen = GetModuleFileNameW((HMODULE)module, tmp, size);
    unsigned long ret = WideCharToMultiByte(CP_UTF8, 0, tmp, tmplen + 1, filename, size, NULL, NULL);
    free(tmp);
    return ret - 1;
}

void pushprogdir(lua_State *L) {
    char buff[MAX_PATH + 1];
    char *lb;
    DWORD nsize = sizeof(buff) / sizeof(char);
    DWORD n = utf8_GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
    if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
        luaL_error(L, "unable to get ModuleFileName");
    else {
        lua_pushlstring(L, buff, lb - buff + 1);
    }
}

#else

#include <dlfcn.h>
#include <string.h>

static const char* progdir() {
    ::Dl_info dl_info;
    dl_info.dli_fname = 0;
    int const ret = ::dladdr((void*)&lua_newstate, &dl_info);
    if (0 != ret) {
        return dl_info.dli_fname;
    }
    return 0;
}

void pushprogdir(lua_State *L) {
    const char* buff = progdir();
    const char *lb;
    if (!buff || (lb = strrchr(buff, '/')) == NULL)
        luaL_error(L, "unable to get dli_fname");
    else {
        lua_pushlstring(L, buff, lb - buff + 1);
    }
}

#endif
