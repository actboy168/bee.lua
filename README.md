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
* Add error hook (for debugger)
* Add resume/yield hook (for debugger)
