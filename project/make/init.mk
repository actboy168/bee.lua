
ifeq "$(PLAT)" ""
	ifeq "$(shell uname -s | cut -c 1-7)" "MSYS_NT"
		PLAT = mingw
	else ifeq "$(shell uname -s)" "Linux"
		PLAT = linux
	else ifeq "$(shell uname -s)" "Darwin"
		PLAT = macos
	endif
endif

BUILD_CONFIG = Release
CC = gcc -std=c11
CXX = g++ -std=c++17
ifeq "$(BUILD_CONFIG)" "Release"
DEBUG_INFO = -O2
else
DEBUG_INFO = -g -D_DEBUG
endif

LUADIR = 3rd/lua/src
LUASERIDIR = 3rd/lua-seri

CFLAGS = $(DEBUG_INFO) -Wall -Wextra

ifeq "$(PLAT)" "mingw"

LUAFLAGS = -DLUA_BUILD_AS_DLL
LUALIB = -llua54
LDSHARED = --shared
STRIP = strip --strip-unneeded

else ifeq "$(PLAT)" "linux"

CFLAGS += -fPIC
LUA_FLAGS = -DLUA_USE_LINUX -DLUA_USE_READLINE
LUALIB = -llua
LDSHARED = --shared
STRIP = strip --strip-unneeded

else ifeq "$(PLAT)" "macosx"

LUA_FLAGS = -DLUA_USE_MACOSX
LUALIB = -llua
LDSHARED = -fPIC -dynamiclib -Wl,-undefined,dynamic_lookup
STRIP = strip -u -r -x

endif
