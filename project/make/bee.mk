BEE_COMMON = $(TMPDIR)/bee_error.o

ifeq "$(PLAT)" "mingw"
BEE_COMMON += $(TMPDIR)/bee_error_windows_category.o
BEE_COMMON += $(TMPDIR)/bee_utility_unicode.o
endif

BEE_FILESYSTEM = \
	$(TMPDIR)/binding_lua_filesystem.o \
	$(TMPDIR)/bee_utility_path_helper.o \
	$(BEE_COMMON)

BEE_FILEWATCH = \
	$(TMPDIR)/binding_lua_filewatch.o \
	$(TMPDIR)/bee_fsevent_fsevent_win.o \
	$(BEE_COMMON)

BEE_REGISTRY = \
	$(TMPDIR)/binding_lua_registry.o \
	$(BEE_COMMON)

BEE_SOCKET = \
	$(TMPDIR)/binding_lua_socket.o \
	$(TMPDIR)/bee_net_socket.o \
	$(TMPDIR)/bee_net_endpoint.o \
	$(TMPDIR)/bee_net_unixsocket.o \
	$(TMPDIR)/bee_platform_version.o \
	$(TMPDIR)/bee_utility_file_version.o \
	$(BEE_COMMON)

BEE_SUBPROCESS = \
	$(TMPDIR)/binding_lua_subprocess.o \
	$(TMPDIR)/bee_subprocess_subprocess_win.o \
	$(BEE_COMMON)

BEE_THREAD = \
	$(TMPDIR)/binding_lua_thread.o \
	$(TMPDIR)/lua-seri.o \
	$(BEE_COMMON)

BEE_UNICODE = \
	$(TMPDIR)/binding_lua_unicode.o \
	$(BEE_COMMON)

BEE_SERIALIZATION = \
	$(TMPDIR)/binding_lua_serialization.o \
	$(TMPDIR)/lua-seri.o \
	$(BEE_COMMON)

BEE_PLATFORM = \
	$(TMPDIR)/binding_lua_platform.o \

BEE_ALL = \
	$(BEE_FILESYSTEM) \
	$(BEE_FILEWATCH) \
	$(BEE_REGISTRY) \
	$(BEE_SOCKET) \
	$(BEE_SUBPROCESS) \
	$(BEE_THREAD) \
	$(BEE_SERIALIZATION) \
	$(BEE_PLATFORM)


ifeq "$(PLAT)" "mingw"
BEE_ALL += $(BEE_UNICODE)
endif

ifeq "$(PLAT)" "linux"
BEE_ALL = \
	$(BEE_THREAD) \
	$(BEE_SERIALIZATION) \
	$(BEE_PLATFORM)
endif
