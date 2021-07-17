# bee.lua

Lua runtime and toolset

[![Build Status](https://github.com/actboy168/bee.lua/workflows/build/badge.svg)](https://github.com/actboy168/bee.lua/actions?workflow=build)

## Build

* install [luamake](https://github.com/actboy168/luamake)
* `> luamake`

## Lua patch

* Enable ansi escape code on windows
* String encoding on windows using utf8
* Remove randomness when traversing the table
* Add error hook (for debugger)
* Add resume/yield hook (for debugger)
