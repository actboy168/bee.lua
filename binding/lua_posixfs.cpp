#include <lua.hpp>
#include <filesystem>
#include <bee/utility/path_helper.h>
#include <bee/lua/binding.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <utime.h>

namespace bee::lua_posixfs {
    static int lgetcwd(lua_State* L) {
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        size_t size = 256; 
        for (;;) {
            char* path = luaL_prepbuffsize(&b, size);
            if (getcwd(path, size) != NULL) {
                lua_pushstring(L, path);
                return 1;
            }
            if (errno != ERANGE) {
                lua_pushstring(L, "getcwd() failed");
                return lua_error(L);
            }
            size *= 2;
        }
    }

    static int lstat_type(lua_State* L) {
        struct stat info;
        if (stat(luaL_checkstring(L, 1), &info)) {
            return 0;
        }
        if (S_ISDIR(info.st_mode)) {
            lua_pushstring(L, "dir");
            return 1;
        }
        if (S_ISREG(info.st_mode)) {
            lua_pushstring(L, "file");
            return 1;
        }
        lua_pushstring(L, "unknown");
        return 1;
    }

#define PERMS_MASK (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)

    static int lstat_perms(lua_State* L) {
        struct stat info;
        if (stat(luaL_checkstring(L, 1), &info)) {
            return 0;
        }
        lua_pushinteger(L, info.st_mode & PERMS_MASK);
        return 1;
    }

    static int lstat_mtime(lua_State* L) {
        struct stat info;
        if (stat(luaL_checkstring(L, 1), &info)) {
            return 0;
        }
        lua_pushinteger(L, info.st_mtime);
        return 1;
    }

    static int lutime(lua_State* L) {
        struct stat info;
        if (stat(luaL_checkstring(L, 1), &info)) {
            lua_pushboolean(L, 0);
            return 1;
        }
        utimbuf buf;
        buf.actime = info.st_atime;
        buf.modtime = luaL_checkinteger(L, 2);
        if (utime(luaL_checkstring(L, 1), &buf)) {
            lua_pushboolean(L, 0);
            return 1;
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int lmkdir(lua_State* L) {
        int res = mkdir(luaL_checkstring(L, 1), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
        lua_pushboolean(L, res == 0);
        return 1;
    }

    static int lchmod(lua_State* L) {
        int res = chmod(luaL_checkstring(L, 1), PERMS_MASK & luaL_checkinteger(L, 2));
        lua_pushboolean(L, res == 0);
        return 1;
    }

    struct dirIter {
        int  closed;
        DIR* dir;
    };

    static int ldirnext(lua_State* L) {
        dirIter* d = (dirIter*)(lua_touserdata(L, lua_upvalueindex(1)));
        luaL_argcheck(L, d->closed == 0, 1, "closed directory");
        struct dirent* entry;
        if ((entry = readdir(d->dir)) != NULL) {
            lua_pushstring(L, entry->d_name);
            return 1;
        }
        else {
            closedir(d->dir);
            d->closed = 1;
            return 0;
        }
    }

    static int ldirclose (lua_State* L) {
        dirIter* d = (dirIter*)lua_touserdata(L, 1);
        if (!d->closed && d->dir) {
            closedir(d->dir);
        }
        d->closed = 1;
        return 0;
    }

    static int ldir(lua_State* L) {
        const char* path = luaL_checkstring (L, 1);
        dirIter* d = (dirIter*)lua_newuserdata(L, sizeof(dirIter));
        lua_newtable(L);
        lua_pushcclosure(L, ldirclose, 0);
        lua_setfield(L, -2, "__gc");
        lua_setmetatable(L, -2);
        lua_pushcclosure(L, ldirnext, 1);
        d->closed = 0;
        d->dir = opendir(path);
        if (d->dir == NULL) {
            luaL_error(L, "cannot open %s: %s", path, strerror(errno));
        }
        return 1;
    }

    static int lexe_path(lua_State* L)  {
        LUA_TRY;
        lua_pushstring(L,path_helper::exe_path().value().string().c_str());
        return 1;
        LUA_TRY_END;
    }

    static int ldll_path(lua_State* L)  {
        LUA_TRY;
        lua_pushstring(L, path_helper::dll_path().value().string().c_str());
        return 1;
        LUA_TRY_END;
    }

    int luaopen(lua_State* L) {
        static luaL_Reg lib[] = {
            { "getcwd", lgetcwd },
            { "stat_type", lstat_type },
            { "stat_perms", lstat_perms },
            { "stat_mtime", lstat_mtime },
            { "mkdir", lmkdir },
            { "chmod", lchmod },
            { "utime", lutime },
            { "dir", ldir },
            { "exe_path", lexe_path },
            { "dll_path", ldll_path },
            { NULL, NULL }
        };
        luaL_newlib(L, lib);
        return 1;
    }
}

DEFINE_LUAOPEN(posixfs)
