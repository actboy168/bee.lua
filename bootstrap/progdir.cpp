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
        luaL_error(L, "unable to get progdir");
    else {
        lua_pushlstring(L, buff, lb - buff + 1);
    }
}

#elif defined(__APPLE__)

#include <mach-o/dyld.h>
#include <stdlib.h>
#include <string.h>

void pushprogdir(lua_State *L) {
    uint32_t bufsize = 0;
    _NSGetExecutablePath(0, &bufsize);
    if (bufsize <= 1) {
        luaL_error(L, "unable to get progdir");
        return;
    }
    char* linkname = (char*)malloc(bufsize+1);
    int rv = _NSGetExecutablePath(linkname, &bufsize);
    if (rv != 0) {
        free(linkname);
        luaL_error(L, "unable to get progdir");
        return;
    }
    linkname[bufsize-1] = '\0';
    const char* lb = strrchr(linkname, '/');
    if (lb) {
        lua_pushlstring(L, linkname, lb - linkname + 1);
    }
    else {
        lua_pushstring(L, linkname);
    }
    free(linkname);
}

#elif defined(__linux__) || defined(__NetBSD__)

#include <unistd.h>
#include <memory.h>

void pushprogdir(lua_State *L) {
    char linkname[1024];
    ssize_t r = readlink("/proc/self/exe", linkname, sizeof(linkname)-1);
    if (r < 0 || r == sizeof(linkname)-1) {
        luaL_error(L, "unable to get progdir");
        return;
    }
    linkname[r] = '\0';
    const char* lb = strrchr(linkname, '/');
    if (lb) {
        lua_pushlstring(L, linkname, lb - linkname + 1);
    }
    else {
        lua_pushstring(L, linkname);
    }
}

#elif defined(__FreeBSD__)

#include <sys/param.h>
#include <sys/sysctl.h>
#include <memory.h>

void pushprogdir(lua_State *L) {
    int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    char linkname[1024];
    size_t length = 1024;
    int error = sysctl(name, 4, linkname, &length, NULL, 0);
    if (error < 0 || length <= 1) {
        luaL_error(L, "unable to get progdir");
        return;
    }
    linkname[length] = '\0';
    const char* lb = strrchr(linkname, '/');
    if (lb) {
        lua_pushlstring(L, linkname, lb - linkname + 1);
    }
    else {
        lua_pushstring(L, linkname);
    }
}

#elif defined(__OpenBSD__)

#include <sys/sysctl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void pushprogdir(lua_State *L) {
    int name[] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };
    size_t argc;
    if (sysctl(name, 4, NULL, &argc, NULL, 0) < 0) {
        luaL_error(L, "unable to get progdir");
        return;
    }
    const char** argv = (const char**)malloc(argc);
    if (!argv) {
        luaL_error(L, "unable to get progdir");
        return;
    }
    if (sysctl(name, 4, argv, &argc, NULL, 0) < 0) {
        luaL_error(L, "unable to get progdir");
        return;
    }
    const char* linkname = argv[0];
    const char* lb = strrchr(linkname, '/');
    if (lb) {
        lua_pushlstring(L, linkname, lb - linkname + 1);
    }
    else {
        lua_pushstring(L, linkname);
    }
    free(argv);
}

#else
void pushprogdir(lua_State *L) {
    luaL_error(L, "unable to get progdir");
}
#endif
