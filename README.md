# bee.lua

Lua runtime and toolset

| MSVC 15.8 (Windows) | MinGW 8.2 (Windows) | Clang 10.0 (macOS) | GCC 8.2 (Linux) |
| ------ | ------ | ------ | ------ |
| [![Build Status](https://dev.azure.com/actboy168/bee.lua/_apis/build/status/bee.lua?branchName=master&jobName=windows_msvc)](https://dev.azure.com/actboy168/bee.lua/_build/latest?definitionId=5?branchName=master) | [![Build Status](https://dev.azure.com/actboy168/bee.lua/_apis/build/status/bee.lua?branchName=master&jobName=Windows_MinGW)](https://dev.azure.com/actboy168/bee.lua/_build/latest?definitionId=5?branchName=master) | [![Build Status](https://dev.azure.com/actboy168/bee.lua/_apis/build/status/bee.lua?branchName=master&jobName=macos)](https://dev.azure.com/actboy168/bee.lua/_build/latest?definitionId=5?branchName=master) | [![Build status](https://ci.appveyor.com/api/projects/status/qfp4flrsoi1aat41?svg=true)](https://ci.appveyor.com/project/actboy168/bee-lua) |

## Build

* install ninja
* ninja -f build/msvc/make.ninja
* ninja -f build/mingw/make.ninja
* ninja -f build/linux/make.ninja
* ninja -f build/macos/make.ninja

## Lua patch

* Enable ansi escape code on windows
* String encoding on windows using utf8
* Remove randomness when traversing the table
* Disable load binary chunk (for security)
* Add error hooks (for debuggers)
* Add lua_getprotohash (for debuggers)

## Feature

|            | Windows | Linux | macOS |
| ---------- | ------- | ----- |------ |
| filesystem |   Yes   |  Yes  |  Yes  |
| thread     |   Yes   |  Yes  |  Yes  |
| socket     |   Yes   |  Yes  |  Yes  |
| subprocess |   Yes   |  Yes  |  Yes  |
| filewatch  |   Yes   | TODO  |  Yes  |
| registry   |   Yes   |  N/A  |  N/A  |

## TODO

* Linux filewatch
* High-level network library
