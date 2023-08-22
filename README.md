# bee.lua

Lua runtime and toolset

[![test](https://github.com/actboy168/bee.lua/actions/workflows/test.yml/badge.svg)](https://github.com/actboy168/bee.lua/actions/workflows/test.yml)

## Build

* install [luamake](https://github.com/actboy168/luamake)
*
  + `> luamake` (all in one)
  + `> luamake -EXE lua` (with `bee.dll`)

## Lua patch

* Enable ansi escape code on windows
* String encoding on windows using utf8
* Remove randomness when traversing the table
* Fast setjmp on windows
* Add error hook (for debugger)
* Add resume/yield hook (for debugger)
* Enable lua_assert in debug mode
* Disable tail calls in debug mode (for debugger)

## 3rd Party Libraries

* [lua/lua](https://github.com/lua/lua) Lua.
* [cloudwu/lua-seri](https://github.com/cloudwu/ltask/blob/master/src/lua-seri.c) Lua serialize.
* [fmtlib/fmt](https://github.com/fmtlib/fmt) Compatible with `std::format`(c++20) and `std::print`(c++23).
* [gulrak/filesystem](https://github.com/gulrak/filesystem) Compatible with `std::filesystem`(c++17).
* [actboy168/ltest](https://github.com/actboy168/ltest) Test framework.
