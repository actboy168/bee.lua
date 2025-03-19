#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#    define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_WIN32)
#    include <3rd/lua-patch/bee_utf8_crt.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.hpp>
#include <string_view>

#if LUA_VERSION_NUM >= 505
#    if defined(_WIN32)
#        define lua_writestringerror(s, p) utf8_ConsoleError(s, p)
#    else
#        define lua_writestringerror(s, p) (fprintf(stderr, (s), (p)), fflush(stderr))
#    endif
#endif

#if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#if !defined(LUA_PROGNAME)
#    define LUA_PROGNAME "lua"
#endif

static lua_State *globalL = NULL;

static const char *progname = LUA_PROGNAME;
/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop(lua_State *L, lua_Debug *ar) {
    (void)ar;                   /* unused arg. */
    lua_sethook(L, NULL, 0, 0); /* reset hook */
    luaL_error(L, "interrupted!");
}

/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lua state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction(int i) {
    signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
    lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message(const char *pname, const char *msg) {
    if (pname) lua_writestringerror("%s: ", pname);
    lua_writestringerror("%s\n", msg);
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report(lua_State *L, int status) {
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        l_message(progname, msg);
        lua_pop(L, 1); /* remove message */
    }
    return status;
}

/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) {                           /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)      /* that produces a string? */
            return 1;                            /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1); /* append a standard traceback */
    return 1;                     /* return the traceback */
}

/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall(lua_State *L, int narg, int nres) {
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler); /* push message handler */
    lua_insert(L, base);              /* put it under function and args */
    globalL = L;                      /* to be available to 'laction' */
    signal(SIGINT, laction);          /* set C-signal handler */
    status = lua_pcall(L, narg, nres, base);
    signal(SIGINT, SIG_DFL); /* reset C-signal handler */
    lua_remove(L, base);     /* remove message handler from the stack */
    return status;
}

static void createargtable(lua_State *L, char **argv, int argc) {
    int i;
    lua_createtable(L, argc - 1, 2);
    lua_pushstring(L, argv[0]);
    lua_rawseti(L, -2, -1);
    lua_pushstring(L, "!main.lua");
    lua_rawseti(L, -2, 0);
    for (i = 1; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "arg");
}

/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs(lua_State *L) {
    int i, n;
    if (lua_getglobal(L, "arg") != LUA_TTABLE)
        luaL_error(L, "'arg' is not a table");
    n = (int)luaL_len(L, -1);
    luaL_checkstack(L, n + 3, "too many arguments to script");
    for (i = 1; i <= n; i++)
        lua_rawgeti(L, -i, i);
    lua_remove(L, -i); /* remove table from the stack */
    return n;
}

static constexpr std::string_view bootstrap = R"BOOTSTRAP(
    local function loadfile(filename)
        local file  = assert(io.open(filename))
        local str  = file:read "a"
        file:close()
        return load(str, "=(main.lua)")
    end
    local sys = require "bee.sys"
    local platform = require "bee.platform"
    local progdir = sys.exe_path():parent_path()
    local mainlua = (progdir / "main.lua"):string()
    if platform.os == "windows" then
        package.cpath = (progdir / "?.dll"):string()
    else
        package.cpath = (progdir / "?.so"):string()
    end
    assert(loadfile(mainlua))(...)
)BOOTSTRAP";

static int handle_script(lua_State *L) {
    int status = luaL_loadbuffer(L, bootstrap.data(), bootstrap.size(), "=(bootstrap.lua)");
    if (status == LUA_OK) {
        int n  = pushargs(L);
        status = docall(L, n, 0);
    }
    return report(L, status);
}

/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain(lua_State *L) {
    int argc    = (int)lua_tointeger(L, 1);
    char **argv = (char **)lua_touserdata(L, 2);
    luaL_checkversion(L); /* check that interpreter has correct version */
    if (argv[0] && argv[0][0]) progname = argv[0];
    lua_pushboolean(L, 1); /* signal for libraries to ignore env. vars. */
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
    luaL_openlibs(L);              /* open standard libraries */
    createargtable(L, argv, argc); /* create table 'arg' */
    lua_gc(L, LUA_GCGEN, 0, 0);    /* GC in generational mode */
    if (handle_script(L) != LUA_OK)
        return 0;
    lua_pushboolean(L, 1); /* signal no errors */
    return 1;
}

#if defined(_WIN32)
extern "C" {
#    include "3rd/lua-patch/bee_utf8_main.c"
}
extern "C" int utf8_main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
    int status, result;
    lua_State *L = luaL_newstate(); /* create state */
    if (L == NULL) {
        l_message(argv[0], "cannot create state: not enough memory");
        return EXIT_FAILURE;
    }
    lua_pushcfunction(L, &pmain);   /* to call 'pmain' in protected mode */
    lua_pushinteger(L, argc);       /* 1st argument */
    lua_pushlightuserdata(L, argv); /* 2nd argument */
    status = lua_pcall(L, 2, 1, 0); /* do the call */
    result = lua_toboolean(L, -1);  /* get result */
    report(L, status);
    lua_close(L);
    return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
