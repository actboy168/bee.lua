#pragma once

#define BEE_STRINGIZE(_x) BEE_STRINGIZE_(_x)
#define BEE_STRINGIZE_(_x) #_x

// see http://sourceforge.net/apps/mediawiki/predef/index.php?title=Operating_Systems
#if defined(_WIN32)
#	ifndef _WIN32_WINNT
#		error "_WIN32_WINNT* is not defined!"
#	endif
#	define BEE_OS_WINDOWS _WIN32_WINNT
#	define BEE_OS_NAME "Windows"
#elif defined(__ANDROID__)
#	include <sys/cdefs.h>
#	define BEE_OS_ANDROID __ANDROID_API__
#	define BEE_OS_NAME "Android"
#elif defined(__linux__)
#	define BEE_OS_LINUX 1
#	define BEE_OS_NAME "Linux"
#elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#	define BEE_OS_IOS __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#	define BEE_OS_NAME "iOS"
#elif defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)
#	define BEE_OS_IOS __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__
#	define BEE_OS_NAME "iOS"
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#	define BEE_OS_OSX __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#	define BEE_OS_NAME "macOS"
#else
#	error "BEE_OS_* is not defined!"
#endif

#if defined(__clang__)
// clang defines __GNUC__ or _MSC_VER
#	define BEE_COMPILER_CLANG (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#	define BEE_COMPILER_NAME "clang"
#	define BEE_COMPILER_VERSION "Clang " \
		BEE_STRINGIZE(__clang_major__) "." \
		BEE_STRINGIZE(__clang_minor__) "." \
		BEE_STRINGIZE(__clang_patchlevel__)
#elif defined(_MSC_VER)
// see https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
#	define BEE_COMPILER_MSVC _MSC_VER
#	define BEE_COMPILER_NAME "msvc"
#	if _MSC_VER >= 1926
#		define BEE_COMPILER_VERSION "MSVC 16.6"
#	elif _MSC_VER >= 1925
#		define BEE_COMPILER_VERSION "MSVC 16.5"
#	elif _MSC_VER >= 1924
#		define BEE_COMPILER_VERSION "MSVC 16.4"
#	elif _MSC_VER >= 1923
#		define BEE_COMPILER_VERSION "MSVC 16.3"
#	elif _MSC_VER >= 1922
#		define BEE_COMPILER_VERSION "MSVC 16.2"
#	elif _MSC_VER >= 1921
#		define BEE_COMPILER_VERSION "MSVC 16.1"
#	elif _MSC_VER >= 1920
#		define BEE_COMPILER_VERSION "MSVC 16.0"
#	elif _MSC_VER >= 1916
#		define BEE_COMPILER_VERSION "MSVC 15.9"
#	elif _MSC_VER >= 1915
#		define BEE_COMPILER_VERSION "MSVC 15.8"
#	elif _MSC_VER >= 1914
#		define BEE_COMPILER_VERSION "MSVC 15.7"
#	elif _MSC_VER >= 1913
#		define BEE_COMPILER_VERSION "MSVC 15.6"
#	elif _MSC_VER >= 1912
#		define BEE_COMPILER_VERSION "MSVC 15.5"
#	elif _MSC_VER >= 1911
#		define BEE_COMPILER_VERSION "MSVC 15.3"
#	elif _MSC_VER >= 1910
#		define BEE_COMPILER_VERSION "MSVC 15.0"
#	elif _MSC_VER >= 1900
#		define BEE_COMPILER_VERSION "MSVC 14.0"
#	elif _MSC_VER >= 1800
#		define BEE_COMPILER_VERSION "MSVC 12.0"
#	elif _MSC_VER >= 1700
#		define BEE_COMPILER_VERSION "MSVC 11.0"
#	elif _MSC_VER >= 1600
#		define BEE_COMPILER_VERSION "MSVC 10.0"
#	elif _MSC_VER >= 1500
#		define BEE_COMPILER_VERSION "MSVC 9.0"
#	elif _MSC_VER >= 1400
#		define BEE_COMPILER_VERSION "MSVC 8.0"
#	elif _MSC_VER >= 1310
#		define BEE_COMPILER_VERSION "MSVC 7.1"
#	elif _MSC_VER >= 1300
#		define BEE_COMPILER_VERSION "MSVC 7.0"
#	elif _MSC_VER >= 1200
#		define BEE_COMPILER_VERSION "MSVC 6.0"
#	else
#		define BEE_COMPILER_VERSION "MSVC"
#	endif //
#elif defined(__GNUC__)
#	define BEE_COMPILER_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#	define BEE_COMPILER_NAME "gcc"
#	define BEE_COMPILER_VERSION "GCC " \
		BEE_STRINGIZE(__GNUC__) "." \
		BEE_STRINGIZE(__GNUC_MINOR__) "." \
		BEE_STRINGIZE(__GNUC_PATCHLEVEL__)
#else
#	error "BEE_COMPILER_* is not defined!"
#endif

// see https://sourceforge.net/p/predef/wiki/Libraries/
#if defined(__BIONIC__)
#	define BEE_CRT_NAME "bionic"
#	define BEE_CRT_NAME "bionic"
#elif defined(_MSC_VER)
#	define BEE_CRT_NAME "msvc"
#	define BEE_CRT_VERSION BEE_COMPILER_VERSION
#elif defined(__GLIBCXX__)
#	define BEE_CRT_NAME "libstdc++"
#	define BEE_CRT_VERSION "libstdc++ " \
		BEE_STRINGIZE(__GLIBCXX__)
#elif defined(__apple_build_version__) || defined(__ORBIS__) || defined(__EMSCRIPTEN__) || defined(__llvm__)
#	define BEE_CRT_NAME "libc++"
#	define BEE_CRT_VERSION "libc++ " \
		BEE_STRINGIZE(_LIBCPP_VERSION)
#else
#	define BEE_CRT_NAME "none"
#	define BEE_CRT_VERSION "none"
#endif

#if defined(__x86_64__)    \
 || defined(_M_X64)        \
 || defined(__aarch64__)   \
 || defined(__64BIT__)     \
 || defined(__mips64)      \
 || defined(__powerpc64__) \
 || defined(__ppc64__)     \
 || defined(__LP64__)
#	define BEE_ARCH 64
#else
#	define BEE_ARCH 32
#endif

#ifdef NDEBUG
#	define BEE_DEBUG 0
#else
#	define BEE_DEBUG 1
#endif
