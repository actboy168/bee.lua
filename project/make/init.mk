PLAT = mingw

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

LUAFLAGS = -DLUA_BUILD_AS_DLL
LUALIB = -L$(LUADIR) -llua54
LDSHARED = --shared
STRIP = strip --strip-unneeded
CFLAGS = $(DEBUG_INFO) -Wall -Wextra
