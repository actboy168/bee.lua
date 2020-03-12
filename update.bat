chcp 65001
..\luamake\luamake init -rebuilt no -plat msvc
..\luamake\luamake init -rebuilt no -plat mingw
..\luamake\luamake init -rebuilt no -plat linux
..\luamake\luamake init -rebuilt no -plat macos

if not exist ninja (
	md ninja
)
move /Y build\msvc\make.ninja  ninja\msvc.ninja
move /Y build\mingw\make.ninja ninja\mingw.ninja
move /Y build\linux\make.ninja ninja\linux.ninja
move /Y build\macos\make.ninja ninja\macos.ninja
