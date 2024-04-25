#include <bee/lua/file.h>
#include <bee/utility/assume.h>
#include <errno.h>
#include <string.h>

namespace bee::lua {
#if defined(LUA_USE_POSIX)
#    define l_getc(f) getc_unlocked(f)
#    define l_lockfile(f) flockfile(f)
#    define l_unlockfile(f) funlockfile(f)
#else
#    define l_getc(f) getc(f)
#    define l_lockfile(f) ((void)0)
#    define l_unlockfile(f) ((void)0)
#endif

#define luabuf_addchar(B, c)                                  \
    ((void)((B)->n < (B)->size || luaL_prepbuffsize((B), 1)), \
     ((B)->b[(B)->n++] = (c)))
#define luabuf_addsize(B, s) ((B)->n += (s))
#define luabuf_buffsub(B, s) ((B)->n -= (s))

    static luaL_Stream* tolstream(lua_State* L) {
        return (luaL_Stream*)luaL_checkudata(L, 1, "bee::file");
    }
    static bool isclosed(luaL_Stream* p) {
        return p->closef == NULL;
    }
    static FILE* tofile(lua_State* L) {
        luaL_Stream* p = tolstream(L);
        if (isclosed(p))
            luaL_error(L, "attempt to use a closed file");
        lua_assert(p->f);
        return p->f;
    }
    static int test_eof(lua_State* L, FILE* f) {
        int c = getc(f);
        ungetc(c, f);
        lua_pushliteral(L, "");
        return (c != EOF);
    }
    static void read_all(lua_State* L, FILE* f) {
        size_t nr;
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        do {
            char* p = luaL_prepbuffsize(&b, LUAL_BUFFERSIZE);
            nr      = fread(p, sizeof(char), LUAL_BUFFERSIZE, f);
            luabuf_addsize(&b, nr);
        } while (nr == LUAL_BUFFERSIZE);
        luaL_pushresult(&b);
    }
    static int read_chars(lua_State* L, FILE* f, size_t n) {
        size_t nr;
        char* p;
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        p  = luaL_prepbuffsize(&b, n);
        nr = fread(p, sizeof(char), n, f);
        luabuf_addsize(&b, nr);
        luaL_pushresult(&b);
        return (nr > 0);
    }
    static int read_line(lua_State* L, FILE* f, int chop) {
        luaL_Buffer b;
        int c;
        luaL_buffinit(L, &b);
        do {
            char* buff = luaL_prepbuffsize(&b, LUAL_BUFFERSIZE);
            int i      = 0;
            l_lockfile(f);
            while (i < LUAL_BUFFERSIZE && (c = l_getc(f)) != EOF && c != '\n')
                buff[i++] = (char)c;
            l_unlockfile(f);
            luabuf_addsize(&b, i);
        } while (c != EOF && c != '\n');
        if (!chop && c == '\n')
            luabuf_addchar(&b, (char)c);
        luaL_pushresult(&b);
        return (c == '\n' || lua_rawlen(L, -1) > 0);
    }
    static int f_read(lua_State* L) {
        FILE* f     = tofile(L);
        int success = 1;
        clearerr(f);
        if (lua_type(L, 2) == LUA_TNUMBER) {
            size_t l = (size_t)luaL_checkinteger(L, 2);
            success  = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
        } else {
            const char* p = luaL_checkstring(L, 2);
            if (*p == 'a') {
                read_all(L, f);
            } else {
                return luaL_argerror(L, 2, "invalid format");
            }
        }
        if (ferror(f))
            return luaL_fileresult(L, 0, NULL);
        if (!success) {
            lua_pop(L, 1);
            lua_pushnil(L);
        }
        return 1;
    }
    static int f_write(lua_State* L) {
        FILE* f    = tofile(L);
        int status = 1;
        if (lua_type(L, 2) == LUA_TNUMBER) {
            int len = lua_isinteger(L, 2)
                          ? fprintf(f, LUA_INTEGER_FMT, (LUAI_UACINT)lua_tointeger(L, 2))
                          : fprintf(f, LUA_NUMBER_FMT, (LUAI_UACNUMBER)lua_tonumber(L, 2));
            status  = len > 0;
        } else {
            size_t l;
            const char* s = luaL_checklstring(L, 2, &l);
            status        = fwrite(s, sizeof(char), l, f) == l;
        }
        if (status) {
            lua_pushvalue(L, 1);
            return 1;
        }
        return luaL_fileresult(L, status, NULL);
    }
    static int g_read(lua_State* L, FILE* f, int first) {
        int success;
        clearerr(f);
        success = read_line(L, f, 1);
        if (ferror(f))
            return luaL_fileresult(L, 0, NULL);
        if (!success) {
            lua_pop(L, 1);
            lua_pushnil(L);
        }
        return 1;
    }
    static int io_readline(lua_State* L) {
        luaL_Stream* p = (luaL_Stream*)lua_touserdata(L, lua_upvalueindex(1));
        if (isclosed(p))
            return luaL_error(L, "file is already closed");
        lua_settop(L, 1);
        int n = g_read(L, p->f, 2);
        lua_assert(n > 0);
        if (lua_toboolean(L, -n))
            return n;
        else {
            if (n > 1) {
                return luaL_error(L, "%s", lua_tostring(L, -n + 1));
            }
            return 0;
        }
    }
    static int f_lines(lua_State* L) {
        tofile(L);
        lua_pushvalue(L, 1);
        lua_pushcclosure(L, io_readline, 1);
        return 1;
    }
    static int _fileclose(lua_State* L) {
        luaL_Stream* p = tolstream(L);
        int ok         = fclose(p->f);
        int en         = errno;
        if (ok) {
            lua_pushboolean(L, 1);
            return 1;
        } else {
            lua_pushnil(L);
#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996)
#endif
            lua_pushfstring(L, "%s", strerror(en));
#if defined(_MSC_VER)
#    pragma warning(pop)
#endif
            lua_pushinteger(L, en);
            return 3;
        }
    }
    static int f_flush(lua_State* L) {
        return luaL_fileresult(L, fflush(tofile(L)) == 0, NULL);
    }
    static int aux_close(lua_State* L) {
        luaL_Stream* p            = tolstream(L);
        volatile lua_CFunction cf = p->closef;
        p->closef                 = NULL;
        return (*cf)(L);
    }
    static int f_close(lua_State* L) {
        tofile(L);
        return aux_close(L);
    }
    static int f_setvbuf(lua_State* L) {
        static const int mode[]              = { _IONBF, _IOFBF, _IOLBF };
        static const char* const modenames[] = { "no", "full", "line", NULL };
        FILE* f                              = tofile(L);
        int op                               = luaL_checkoption(L, 2, NULL, modenames);
        lua_Integer sz                       = luaL_optinteger(L, 3, LUAL_BUFFERSIZE);
        int res                              = setvbuf(f, NULL, mode[op], (size_t)sz);
        return luaL_fileresult(L, res == 0, NULL);
    }
    static int f_gc(lua_State* L) {
        luaL_Stream* p = tolstream(L);
        if (!isclosed(p) && p->f != NULL)
            aux_close(L);
        return 0;
    }
    static int f_tostring(lua_State* L) {
        luaL_Stream* p = tolstream(L);
        if (isclosed(p))
            lua_pushliteral(L, "file (closed)");
        else
            lua_pushfstring(L, "file (%p)", p->f);
        return 1;
    }
    int newfile(lua_State* L, FILE* f) {
        luaL_Stream* pf = (luaL_Stream*)lua_newuserdatauv(L, sizeof(luaL_Stream), 0);
        pf->closef      = &_fileclose;
        pf->f           = f;
        if (luaL_newmetatable(L, "bee::file")) {
            const luaL_Reg meth[] = {
                { "read", f_read },
                { "write", f_write },
                { "lines", f_lines },
                { "flush", f_flush },
                //{"seek", f_seek},
                { "close", f_close },
                { "setvbuf", f_setvbuf },
                { NULL, NULL }
            };
            const luaL_Reg metameth[] = {
                { "__index", NULL },
                { "__gc", f_gc },
                { "__close", f_gc },
                { "__tostring", f_tostring },
                { NULL, NULL }
            };
            luaL_setfuncs(L, metameth, 0);
            lua_createtable(L, 0, sizeof(meth) / sizeof(meth[0]) - 1);
            luaL_setfuncs(L, meth, 0);
            lua_setfield(L, -2, "__index");
        }
        lua_setmetatable(L, -2);
        return 1;
    }

    luaL_Stream* tofile(lua_State* L, int idx) {
        void* p = lua_touserdata(L, idx);
        void* r = NULL;
        if (p) {
            if (lua_getmetatable(L, idx)) {
                do {
                    luaL_getmetatable(L, "bee::file");
                    if (lua_rawequal(L, -1, -2)) {
                        r = p;
                        break;
                    }
                    lua_pop(L, 1);
                    luaL_getmetatable(L, LUA_FILEHANDLE);
                    if (lua_rawequal(L, -1, -2)) {
                        r = p;
                        break;
                    }
                } while (false);
                lua_pop(L, 2);
            }
        }
        luaL_argexpected(L, r != NULL, idx, LUA_FILEHANDLE);
        BEE_ASSUME(r != NULL);
        return (luaL_Stream*)r;
    }
}
