chcp 65001
..\luamake\luamake init -rebuilt no -p msvc
..\luamake\luamake init -rebuilt no -p mingw
..\luamake\luamake init -rebuilt no -p linux
..\luamake\luamake init -rebuilt no -p macos

if not exist ninja (
	md ninja
)
copy /Y build\msvc\make.ninja  ninja\msvc.ninja
copy /Y build\mingw\make.ninja ninja\mingw.ninja
copy /Y build\linux\make.ninja ninja\linux.ninja
copy /Y build\macos\make.ninja ninja\macos.ninja
