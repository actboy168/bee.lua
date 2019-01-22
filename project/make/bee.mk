BEE_COMMON = $(TMPDIR)/bee_error.o

BEE_FILESYSTEM = \
	$(BEE_COMMON)

BEE_FILEWATCH = \
	$(TMPDIR)/binding_lua_filewatch.o \
	$(BEE_COMMON)

BEE_REGISTRY = \
	$(TMPDIR)/binding_lua_registry.o \
	$(BEE_COMMON)

BEE_SOCKET = \
	$(TMPDIR)/binding_lua_socket.o \
	$(TMPDIR)/bee_net_socket.o \
	$(TMPDIR)/bee_net_endpoint.o \
	$(BEE_COMMON)

BEE_SUBPROCESS = \
	$(TMPDIR)/binding_lua_subprocess.o \
	$(TMPDIR)/bee_utility_file_helper.o \
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


ifeq "$(PLAT)" "mingw"

BEE_COMMON += $(TMPDIR)/bee_error_category_win.o
BEE_COMMON += $(TMPDIR)/bee_utility_unicode_win.o
BEE_FILEWATCH += $(TMPDIR)/bee_fsevent_fsevent_win.o
BEE_SUBPROCESS += $(TMPDIR)/bee_net_unixsocket_win.o
BEE_SUBPROCESS += $(TMPDIR)/bee_subprocess_subprocess_win.o
BEE_SUBPROCESS += $(TMPDIR)/bee_subprocess_sharedmemory_win.o
BEE_SOCKET += $(TMPDIR)/bee_utility_module_version_win.o
BEE_SOCKET += $(TMPDIR)/bee_platform_version_win.o
BEE_FILESYSTEM += $(TMPDIR)/binding_lua_filesystem.o
BEE_FILESYSTEM += $(TMPDIR)/bee_utility_path_helper.o

else 

BEE_SUBPROCESS += $(TMPDIR)/bee_subprocess_subprocess_posix.o

ifeq "$(PLAT)" "linux"
BEE_FILESYSTEM += $(TMPDIR)/binding_lua_filesystem.o
BEE_FILESYSTEM += $(TMPDIR)/bee_utility_path_helper.o
else ifeq "$(PLAT)" "macos"
BEE_FILESYSTEM += $(TMPDIR)/binding_lua_posixfs.o
BEE_FILEWATCH += $(TMPDIR)/bee_fsevent_fsevent_osx.o
endif

endif


BEE_ALL = \
	$(TMPDIR)/binding_lua_embed.o \
	$(BEE_FILESYSTEM) \
	$(BEE_SOCKET) \
	$(BEE_SUBPROCESS) \
	$(BEE_THREAD) \
	$(BEE_SERIALIZATION) \
	$(BEE_PLATFORM)

ifeq "$(PLAT)" "mingw"
BEE_ALL += $(BEE_REGISTRY)
BEE_ALL += $(BEE_UNICODE)
endif

ifneq "$(PLAT)" "linux"
BEE_ALL += $(BEE_FILEWATCH)
endif
