# bee.lua

Lua runtime and toolset

| MSVC 15.8 (Windows) | MinGW 8.2 (Windows) |
| ------ | ------ |
| [![Build Status](https://dev.azure.com/actboy168/bee.lua/_apis/build/status/bee.lua?branchName=master&jobName=Windows_MSVC_x64)](https://dev.azure.com/actboy168/bee.lua/_build/latest?definitionId=5?branchName=master) | [![Build Status](https://dev.azure.com/actboy168/bee.lua/_apis/build/status/bee.lua?branchName=master&jobName=Windows_MinGW)](https://dev.azure.com/actboy168/bee.lua/_build/latest?definitionId=5?branchName=master) |

## Lua patch

* Enable ansi escape code on windows
* String encoding on windows using utf8
* Remove randomness when traversing the array
* Disable load binary chunk (for security)
* Add error hooks (for debuggers)
* Add lua_getprotohash (for debuggers)

## Feature

|            | Windows | Linux | macOS |
| ---------- | ------- | ----- |------ |
| filesystem |   Yes   | No<sup>[1]</sup> | No<sup>[1]</sup> |
| thread     |   Yes   |  Yes  |  Yes  |
| socket     |   Yes   |  Yes  |  Yes  |
| subprocess |   Yes   |  Yes  |  Yes  |
| filewatch  |   Yes   | TODO  |  Yes  |
| registry   |   Yes   |  N/A  |  N/A  |

[1] Requires compiler support, gcc8+ or clang7+. But most distribution are still using gcc7- or clang6-.

## TODO

* High-level network library
