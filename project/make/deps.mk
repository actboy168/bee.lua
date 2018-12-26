$(TMPDIR)/lua-seri.o : $(3RD)/lua-seri/lua-seri.c $(3RD)/lua-seri/lua-seri.h | $(TMPDIR)
	$(CC) -c $(CFLAGS) -o $@ $< -I$(LUADIR) 

$(TMPDIR)/bee_error_exception.o : bee/error/exception.cpp bee/error/exception.cpp bee/error/exception.h bee/config.h bee/utility/dynarray.h bee/nonstd/span.h bee/utility/unicode.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_error_windows_category.o : bee/error/windows_category.cpp bee/error/windows_category.cpp bee/error/windows_category.h bee/utility/unicode.h bee/config.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_error.o : bee/error.cpp bee/error.cpp bee/error.h bee/config.h bee/error/windows_category.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_fsevent_fsevent_osx.o : bee/fsevent/fsevent_osx.cpp bee/fsevent/fsevent_osx.cpp bee/fsevent/fsevent_osx.h bee/utility/lockqueue.h bee/utility/semaphore.h bee/utility/format.h bee/utility/hybrid_array.h bee/utility/unicode.h bee/config.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_fsevent_fsevent_win.o : bee/fsevent/fsevent_win.cpp bee/fsevent/fsevent_win.cpp bee/fsevent/fsevent_win.h bee/utility/lockqueue.h bee/utility/unicode.h bee/config.h bee/utility/format.h bee/utility/hybrid_array.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_net_endpoint.o : bee/net/endpoint.cpp bee/net/endpoint.cpp bee/net/endpoint.h bee/utility/dynarray.h bee/nonstd/span.h bee/nonstd/expected.h bee/utility/format.h bee/utility/hybrid_array.h bee/utility/unicode.h bee/config.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_net_socket.o : bee/net/socket.cpp bee/net/socket.cpp bee/utility/unicode.h bee/config.h bee/net/unixsocket.h bee/net/socket.h bee/net/endpoint.h bee/utility/dynarray.h bee/nonstd/span.h bee/nonstd/expected.h bee/platform/version.h bee/nonstd/enum.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_net_unixsocket.o : bee/net/unixsocket.cpp bee/net/unixsocket.cpp bee/net/unixsocket.h bee/net/socket.h bee/net/endpoint.h bee/utility/dynarray.h bee/nonstd/span.h bee/nonstd/expected.h bee/utility/unicode.h bee/config.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_platform_version.o : bee/platform/version.cpp bee/platform/version.cpp bee/platform/version.h bee/config.h bee/nonstd/enum.h bee/utility/file_version.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_subprocess_sharedmemory_win.o : bee/subprocess/sharedmemory_win.cpp bee/subprocess/sharedmemory_win.cpp bee/subprocess/sharedmemory_win.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_subprocess_subprocess_posix.o : bee/subprocess/subprocess_posix.cpp bee/subprocess/subprocess_posix.cpp bee/subprocess.h bee/config.h bee/subprocess/subprocess_win.h bee/net/socket.h bee/subprocess/subprocess_posix.h bee/utility/format.h bee/utility/hybrid_array.h bee/utility/unicode.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_subprocess_subprocess_win.o : bee/subprocess/subprocess_win.cpp bee/subprocess/subprocess_win.cpp bee/subprocess.h bee/config.h bee/subprocess/subprocess_win.h bee/net/socket.h bee/subprocess/subprocess_posix.h bee/subprocess/sharedmemory_win.h bee/nonstd/span.h bee/utility/format.h bee/utility/hybrid_array.h bee/utility/unicode.h bee/subprocess/args_helper.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_utility_file_version.o : bee/utility/file_version.cpp bee/utility/file_version.cpp bee/utility/file_version.h bee/config.h bee/utility/format.h bee/utility/hybrid_array.h bee/utility/unicode.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_utility_path_helper.o : bee/utility/path_helper.cpp bee/utility/path_helper.cpp bee/utility/path_helper.h bee/config.h bee/nonstd/expected.h bee/utility/dynarray.h bee/nonstd/span.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/bee_utility_unicode.o : bee/utility/unicode.cpp bee/utility/unicode.cpp bee/utility/unicode.h bee/config.h bee/utility/dynarray.h bee/nonstd/span.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/binding_lua_embed.o : binding/lua_embed.cpp binding/lua_embed.cpp bee/lua/binding.h bee/utility/unicode.h bee/config.h bee/nonstd/embed.h bee/nonstd/span.h bee/nonstd/embed_detail.h script/bee.lua | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I$(3RD)/incbin -I.

$(TMPDIR)/binding_lua_filesystem.o : binding/lua_filesystem.cpp binding/lua_filesystem.cpp bee/utility/path_helper.h bee/config.h bee/nonstd/expected.h bee/utility/unicode.h bee/lua/range.h bee/lua/binding.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_filewatch.o : binding/lua_filewatch.cpp binding/lua_filewatch.cpp bee/fsevent.h bee/fsevent/fsevent_win.h bee/utility/lockqueue.h bee/fsevent/fsevent_osx.h bee/utility/semaphore.h bee/error.h bee/config.h bee/lua/binding.h bee/utility/unicode.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_platform.o : binding/lua_platform.cpp binding/lua_platform.cpp bee/lua/binding.h bee/utility/unicode.h bee/config.h bee/platform.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_registry.o : binding/lua_registry.cpp binding/lua_registry.cpp bee/registry/key.h bee/registry/traits.h bee/registry/value.h bee/utility/dynarray.h bee/nonstd/span.h bee/registry/exception.h bee/config.h bee/registry/predefined_keys.h bee/utility/unicode.h bee/lua/binding.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_serialization.o : binding/lua_serialization.cpp binding/lua_serialization.cpp bee/lua/binding.h bee/utility/unicode.h bee/config.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I$(3RD)/lua-seri -I.

$(TMPDIR)/binding_lua_socket.o : binding/lua_socket.cpp binding/lua_socket.cpp bee/net/unixsocket.h bee/net/socket.h bee/net/endpoint.h bee/utility/dynarray.h bee/nonstd/span.h bee/nonstd/expected.h bee/lua/binding.h bee/utility/unicode.h bee/config.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_subprocess.o : binding/lua_subprocess.cpp binding/lua_subprocess.cpp bee/subprocess.h bee/config.h bee/subprocess/subprocess_win.h bee/net/socket.h bee/subprocess/subprocess_posix.h bee/utility/unicode.h bee/lua/binding.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_thread.o : binding/lua_thread.cpp binding/lua_thread.cpp bee/utility/semaphore.h bee/utility/lockqueue.h bee/lua/binding.h bee/utility/unicode.h bee/config.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I$(3RD)/lua-seri -I.

$(TMPDIR)/binding_lua_unicode.o : binding/lua_unicode.cpp binding/lua_unicode.cpp bee/lua/binding.h bee/utility/unicode.h bee/config.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.
