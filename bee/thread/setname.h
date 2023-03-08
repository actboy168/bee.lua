#pragma once

#if defined(_WIN32)
#   include <windows.h>
#   include <bee/platform/win/unicode.h>
#else
#   include <pthread.h>
#   if defined(__linux__)
#      define BEE_GLIBC (__GLIBC__ * 100 + __GLIBC_MINOR__)
#      if BEE_GLIBC < 212
#          include <sys/prctl.h>
#      endif
#   endif
#   if defined(__FreeBSD__) || defined(__OpenBSD__)
#       include <pthread_np.h>
#   endif
#endif

namespace bee {

#if defined(_MSC_VER)
inline void thread_setname_internal(const char* name) {
	const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
	struct ThreadNameInfo {
		DWORD  type;
		LPCSTR name;
		DWORD  id;
		DWORD  flags;
	};
#pragma pack(pop)
	struct ThreadNameInfo info;
	info.type  = 0x1000;
	info.name  = name;
	info.id    = GetCurrentThreadId();
	info.flags = 0;
	__try {
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	} __except(GetExceptionCode() == MS_VC_EXCEPTION) {
		(void)NULL;
	}
}
#endif

inline void thread_setname(const char* name) {
#if defined(_WIN32)
	using SetThreadDescriptionProc = HRESULT (WINAPI *)(HANDLE, PCWSTR);
	if (HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll")) {
		if (SetThreadDescriptionProc SetThreadDescription = (SetThreadDescriptionProc)GetProcAddress(kernel32, "SetThreadDescription")) {
			SetThreadDescription(GetCurrentThread(), win::u2w(name).c_str());
		}
	}
#if defined(_MSC_VER)
	if (!IsDebuggerPresent()) {
		return;
	}
	thread_setname_internal(name);
#endif

#elif defined(__APPLE__)
	pthread_setname_np(name);
#elif defined(__linux__)
#	if BEE_GLIBC >= 212
		pthread_setname_np(pthread_self(), name);
#	else
		prctl(PR_SET_NAME, name, 0, 0, 0);
#	endif
#elif defined(__NetBSD__)
	pthread_setname_np(pthread_self(), "%s", (void*)name);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	pthread_set_name_np(pthread_self(), name);
#endif
}

}
