#include <bee/platform/version.h>
#include <bee/utility/module_version_win.h>
#include <Windows.h>

namespace bee::platform {

	version get_version() {
		OSVERSIONINFOW osvi = {  };
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
#if defined(_MSC_VER)
#pragma warning(suppress:4996)
		::GetVersionExW(&osvi);
#else
        ::GetVersionExW(&osvi);
#endif

        version v {
            osvi.dwMajorVersion,
            osvi.dwMinorVersion,
            osvi.dwBuildNumber,
            0
		};

        if ((v.major > 6) || (v.major == 6 && v.minor >= 2)) {
			// see
			//   http://msdn.microsoft.com/en-us/library/windows/desktop/ms724451(v=vs.85).aspx
			//   http://msdn.microsoft.com/en-us/library/windows/desktop/ms724429(v=vs.85).aspx
			simple_module_version sfv(L"kernel32.dll", L"ProductVersion", L'.');
			v.major = sfv.major;
			v.minor = sfv.minor;
			v.revision = sfv.revision;
			v.build = sfv.build;
		}
        return v;
	}
}
