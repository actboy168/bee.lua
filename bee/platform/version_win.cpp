#include <bee/platform/version.h>
#include <bee/utility/module_version_win.h>
#include <Windows.h>

namespace bee::platform {

	static WinVer getWinVersion(uint32_t major, uint32_t minor) {
		if ((major == 5) && (minor > 0)) {
			return (minor == 1) ? WinVer::XP : WinVer::Server2003;
		}
		else if (major == 6) {
			switch (minor) {
			case 0:
				return WinVer::Vista;
			case 1:
				return WinVer::Win7;
			case 2:
				return WinVer::Win8;
				break;
			case 3:
			default:
				return WinVer::Win8_1;
			}
		}
		else if (major == 10) {
			return WinVer::Win10;
		}
		else if (major > 6) {
			return WinVer::Win10Later;
		}
		return WinVer::PreXP;
	}

	version get_version() {
		OSVERSIONINFOW osvi = {  };
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
#if defined(_MSC_VER)
#pragma warning(suppress:4996)
		::GetVersionExW(&osvi);
#else
        ::GetVersionExW(&osvi);
#endif

		uint32_t major = osvi.dwMajorVersion;
		uint32_t minor = osvi.dwMinorVersion;
		uint32_t build = osvi.dwBuildNumber;

		if ((major > 6) || (major == 6 && minor >= 2)) {
			// see
			//   http://msdn.microsoft.com/en-us/library/windows/desktop/ms724451(v=vs.85).aspx
			//   http://msdn.microsoft.com/en-us/library/windows/desktop/ms724429(v=vs.85).aspx
			simple_module_version sfv(L"kernel32.dll", L"ProductVersion", L'.');
			major = sfv.major;
			minor = sfv.minor;
			build = sfv.revision;
		}
		return { getWinVersion(major, minor), build };
	}
}
