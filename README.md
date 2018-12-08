# bee.lua

[![Build status](https://actboy168.visualstudio.com/bee.lua/_apis/build/status/bee.lua-CI)](https://actboy168.visualstudio.com/bee.lua/_build/latest?definitionId=2)

Lua runtime and toolset

## Compiler

* msvc 2017+
* mingw 8.2+

## Lua patch

* String encoding on windows using utf8
* Remove randomness when traversing the array
* Disable load binary chunk (for security)
* Add error hooks (for debuggers)
* Add lua_getprotohash (for debuggers)

## Feature

* filesystem
* thread
* socket
* subprocess
* filewatch
* registry

## TODO

* macOS, Linux support
* High-level network library
