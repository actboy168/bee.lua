#pragma once

#define BEE_STRINGIZE(_x) BEE_STRINGIZE_(_x)
#define BEE_STRINGIZE_(_x) #_x

// see http://sourceforge.net/apps/mediawiki/predef/index.php?title=Operating_Systems
#if defined(_WIN32)
#	include <Windows.h>
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
#	define BEE_COMPILER_VERSION "MSVC " \
		BEE_STRINGIZE(_MSC_FULL_VER) "." \
		BEE_STRINGIZE(_MSC_BUILD)

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
#	if __has_include(<android/ndk-version.h>)
#		include <android/ndk-version.h>
#	endif
#	if defined(__NDK_MAJOR__) && defined(__NDK_MINOR__)
#		define BEE_CRT_VERSION "NDK r" \
			BEE_STRINGIZE(__NDK_MAJOR__) "." \
			BEE_STRINGIZE(__NDK_MINOR__)
#	else
#		define BEE_CRT_VERSION "NDK <unknown>"
#	endif
#elif defined(_MSC_VER)
#	define BEE_CRT_NAME "msvc"
#	define BEE_CRT_VERSION "MSVC STL " \
		BEE_STRINGIZE(_MSVC_STL_UPDATE)
#elif defined(__GLIBCXX__)
#	define BEE_CRT_NAME "libstdc++"
#if defined(__GLIBC__)
#	define BEE_CRT_VERSION "libstdc++ " \
		BEE_STRINGIZE(__GLIBCXX__) \
		" glibc " \
		BEE_STRINGIZE(__GLIBC__) "." \
		BEE_STRINGIZE(__GLIBC_MINOR__)
#else
#	define BEE_CRT_VERSION "libstdc++ " \
		BEE_STRINGIZE(__GLIBCXX__)
#endif
#elif defined(__apple_build_version__) || defined(__ORBIS__) || defined(__EMSCRIPTEN__) || defined(__llvm__)
#	define BEE_CRT_NAME "libc++"
#if defined(__GLIBC__)
#	define BEE_CRT_VERSION "libc++ " \
		BEE_STRINGIZE(_LIBCPP_VERSION) \
		" glibc " \
		BEE_STRINGIZE(__GLIBC__) "." \
		BEE_STRINGIZE(__GLIBC_MINOR__)
#else
#	define BEE_CRT_VERSION "libc++ " \
		BEE_STRINGIZE(_LIBCPP_VERSION)
#endif
#else
#	define BEE_CRT_NAME "none"
#	define BEE_CRT_VERSION "none"
#endif

// http://sourceforge.net/apps/mediawiki/predef/index.php?title=Architectures
#if defined(_M_ARM64) || defined(__aarch64__)
#	define BEE_ARCH "arm64"
#elif defined(_M_IX86) || defined(__i386__)
#	define BEE_ARCH "x86"
#elif defined(_M_X64) || defined(__x86_64__)
#	define BEE_ARCH "x86_64"
#else
#	define BEE_ARCH "unknown"
#endif 

#ifdef NDEBUG
#	define BEE_DEBUG 0
#else
#	define BEE_DEBUG 1
#endif
