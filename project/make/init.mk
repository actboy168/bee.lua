
ifeq "$(PLAT)" ""
	ifeq "$(shell uname -s | cut -c 1-7)" "MSYS_NT"
		PLAT = mingw
	else ifeq "$(shell uname -s)" "Linux"
		PLAT = linux
	else ifeq "$(shell uname -s)" "Darwin"
		PLAT = macosx
	endif
endif

ifeq "$(PLAT)" "mingw"
CC = gcc -std=c11
CXX = g++ -std=c++17
else
CC = clang -std=c11
CXX = clang -std=c++17
endif

BUILD_CONFIG = release
ifeq "$(BUILD_CONFIG)" "release"
DEBUG_INFO = -O2
else
DEBUG_INFO = -g -D_DEBUG
endif

3RD = 3rd
LUADIR = $(3RD)/lua/src

CFLAGS = $(DEBUG_INFO) -Wall -Wextra

ifeq "$(PLAT)" "mingw"

LUAFLAGS = -DLUA_BUILD_AS_DLL
LUALIB = -llua54
LDSHARED = --shared
STRIP = strip --strip-unneeded
CFLAGS += -Wno-cast-function-type

else ifeq "$(PLAT)" "linux"

CFLAGS += -fPIC
LUA_FLAGS = -DLUA_USE_LINUX -DLUA_USE_READLINE
LUALIB = -llua
LDSHARED = --shared
STRIP = strip --strip-unneeded

else ifeq "$(PLAT)" "macosx"

LUA_FLAGS = -DLUA_USE_MACOSX -DLUA_USE_READLINE
LUALIB = -llua
LDSHARED = -fPIC -dynamiclib -Wl,-undefined,dynamic_lookup
STRIP = strip -u -r -x

endif
