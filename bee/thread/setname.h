#pragma once

#if defined(_WIN32)
#   include <windows.h>
#   include <malloc.h>
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

inline void thread_setname(const char* name) {
#if defined(_WIN32)
	using SetThreadDescriptionProc = HRESULT (WINAPI *)(HANDLE, PCWSTR);
	SetThreadDescriptionProc SetThreadDescription = (SetThreadDescriptionProc)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription");
	if (SetThreadDescription) {
		size_t size = (strlen(name)+1) * sizeof(wchar_t);
		wchar_t* wname = (wchar_t*)_alloca(size);
		mbstowcs(wname, name, size-2);
		SetThreadDescription(GetCurrentThread(), wname);
	}
#if defined(_MSC_VER)
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
	} __except(EXCEPTION_EXECUTE_HANDLER) {}
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
