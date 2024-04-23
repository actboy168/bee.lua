#include <bee/lua/module.h>
#include <bee/version.h>

#define BEE_STRINGIZE(_x) BEE_STRINGIZE_(_x)
#define BEE_STRINGIZE_(_x) #_x

#if defined(_WIN32)
#    include <Windows.h>
#elif defined(__ANDROID__)
#    include <sys/cdefs.h>
#endif

#if defined(__BIONIC__)
#    if __has_include(<android/ndk-version.h>)
#        include <android/ndk-version.h>
#    endif
#endif

namespace bee::lua_platform {
    static int luaopen(lua_State* L) {
        lua_createtable(L, 0, 16);

#if defined(_WIN32)
        lua_pushstring(L, "windows");
#elif defined(__ANDROID__)
        lua_pushstring(L, "android");
#elif defined(__linux__)
        lua_pushstring(L, "linux");
#elif defined(__NetBSD__)
        lua_pushstring(L, "netbsd");
#elif defined(__FreeBSD__)
        lua_pushstring(L, "freebsd");
#elif defined(__OpenBSD__)
        lua_pushstring(L, "openbsd");
#elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
        lua_pushstring(L, "ios");
#elif defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)
        lua_pushstring(L, "ios");
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
        lua_pushstring(L, "macos");
#else
        lua_pushstring(L, "unknown");
#endif
        lua_setfield(L, -2, "os");

#if defined(__clang__)
        // clang defines __GNUC__ or _MSC_VER
        lua_pushstring(L, "clang");
        lua_pushstring(L, "Clang " BEE_STRINGIZE(__clang_major__) "." BEE_STRINGIZE(__clang_minor__) "." BEE_STRINGIZE(__clang_patchlevel__));
#elif defined(_MSC_VER)
        // see https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
        lua_pushstring(L, "msvc");
        lua_pushstring(L, "MSVC " BEE_STRINGIZE(_MSC_FULL_VER) "." BEE_STRINGIZE(_MSC_BUILD));

#elif defined(__GNUC__)
        lua_pushstring(L, "gcc");
        lua_pushstring(L, "GCC " BEE_STRINGIZE(__GNUC__) "." BEE_STRINGIZE(__GNUC_MINOR__) "." BEE_STRINGIZE(__GNUC_PATCHLEVEL__));
#else
        lua_pushstring(L, "unknown");
        lua_pushstring(L, "unknown");
#endif
        lua_setfield(L, -3, "CompilerVersion");
        lua_setfield(L, -2, "Compiler");

#if defined(__BIONIC__)
        lua_pushstring(L, "bionic");
#    if defined(__NDK_MAJOR__) && defined(__NDK_MINOR__)
        lua_pushstring(L, "NDK r" BEE_STRINGIZE(__NDK_MAJOR__) "." BEE_STRINGIZE(__NDK_MINOR__));
#    else
        lua_pushstring(L, "NDK <unknown>");
#    endif
#elif defined(_MSC_VER)
        lua_pushstring(L, "msvc");
        lua_pushstring(L, "MSVC STL " BEE_STRINGIZE(_MSVC_STL_UPDATE));
#elif defined(__GLIBCXX__)
        lua_pushstring(L, "libstdc++");
#    if defined(__GLIBC__)
        lua_pushstring(L, "libstdc++ " BEE_STRINGIZE(__GLIBCXX__) " glibc " BEE_STRINGIZE(__GLIBC__) "." BEE_STRINGIZE(__GLIBC_MINOR__));
#    else
        lua_pushstring(L, "libstdc++ " BEE_STRINGIZE(__GLIBCXX__));
#    endif
#elif defined(__apple_build_version__) || defined(__ORBIS__) || defined(__EMSCRIPTEN__) || defined(__llvm__)
        lua_pushstring(L, "libc++");
#    if defined(__GLIBC__)
        lua_pushstring(L, "libc++ " BEE_STRINGIZE(_LIBCPP_VERSION) " glibc " BEE_STRINGIZE(__GLIBC__) "." BEE_STRINGIZE(__GLIBC_MINOR__));
#    else
        lua_pushstring(L, "libc++ " BEE_STRINGIZE(_LIBCPP_VERSION));
#    endif
#else
        lua_pushstring(L, "unknown");
        lua_pushstring(L, "unknown");
#endif
        lua_setfield(L, -3, "CRTVersion");
        lua_setfield(L, -2, "CRT");

#if defined(_M_ARM64) || defined(__aarch64__)
        lua_pushstring(L, "arm64");
#elif defined(_M_IX86) || defined(__i386__)
        lua_pushstring(L, "x86");
#elif defined(_M_X64) || defined(__x86_64__)
        lua_pushstring(L, "x86_64");
#elif defined(__arm__)
        lua_pushstring(L, "arm");
#elif defined(__riscv)
        lua_pushstring(L, "riscv");
#elif defined(__wasm32__)
        lua_pushstring(L, "wasm32");
#elif defined(__wasm64__)
        lua_pushstring(L, "wasm64");
#elif defined(__mips64)
        lua_pushstring(L, "mips64el");
#elif defined(__loongarch64)
        lua_pushstring(L, "loongarch64");
#else
        lua_pushstring(L, "unknown");
#endif
        lua_setfield(L, -2, "Arch");

#if defined(NDEBUG)
        lua_pushboolean(L, 0);
#else
        lua_pushboolean(L, 1);
#endif
        lua_setfield(L, -2, "DEBUG");

        auto version = bee::os_version();
        lua_createtable(L, 0, 4);
        lua_pushinteger(L, version.major);
        lua_setfield(L, -2, "major");
        lua_pushinteger(L, version.minor);
        lua_setfield(L, -2, "minor");
        lua_pushinteger(L, version.revision);
        lua_setfield(L, -2, "revision");
        lua_setfield(L, -2, "os_version");
        return 1;
    }
}

DEFINE_LUAOPEN(platform)
