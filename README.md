# bee.lua
Lua runtime and toolset

## Compiler

* MSVC 2017
* MSVC 2019

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
