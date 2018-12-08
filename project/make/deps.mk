$(TMPDIR)/lua-seri.o : $(LUASERIDIR)/lua-seri.c $(LUASERIDIR)/lua-seri.h | $(TMPDIR)
	$(CC) -c $(CFLAGS) -o $@ $< -I$(LUADIR) 

$(TMPDIR)/bee_error_exception.o : bee\error\exception.cpp bee/error/exception.h bee/utility/unicode.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_error_windows_category.o : bee\error\windows_category.cpp bee/error/windows_category.h bee/utility/unicode.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_error.o : bee\error.cpp bee/error.h bee/error/windows_category.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_fsevent_fsevent_osx.o : bee\fsevent\fsevent_osx.cpp bee/fsevent/fsevent_osx.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_fsevent_fsevent_win.o : bee\fsevent\fsevent_win.cpp bee/fsevent/fsevent_win.h bee/utility/unicode.h bee/utility/format.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_net_endpoint.o : bee\net\endpoint.cpp bee/net/endpoint.h bee/utility/format.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_net_socket.o : bee\net\socket.cpp bee/utility/unicode.h bee/net/unixsocket.h bee/net/socket.h bee/net/endpoint.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_net_unixsocket.o : bee\net\unixsocket.cpp bee/net/unixsocket.h bee/utility/unicode.h bee/platform/version.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_platform_version.o : bee\platform\version.cpp bee/platform/version.h bee/utility/file_version.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_subprocess_subprocess_posix.o : bee\subprocess\subprocess_posix.cpp bee/subprocess.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_subprocess_subprocess_win.o : bee\subprocess\subprocess_win.cpp bee/subprocess.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_utility_file_version.o : bee\utility\file_version.cpp bee/utility/file_version.h bee/utility/format.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_utility_path_helper.o : bee\utility\path_helper.cpp bee/utility/path_helper.h bee/utility/dynarray.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/bee_utility_unicode.o : bee\utility\unicode.cpp bee/utility/unicode.h bee/utility/dynarray.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/binding_lua_embed.o : binding\lua_embed.cpp bee/nonstd/embed.h bee/lua/binding.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_filesystem.o : binding\lua_filesystem.cpp bee/utility/path_helper.h bee/utility/unicode.h bee/lua/range.h bee/lua/binding.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_filewatch.o : binding\lua_filewatch.cpp bee/fsevent.h bee/error.h bee/lua/binding.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I.

$(TMPDIR)/binding_lua_registry.o : binding\lua_registry.cpp bee/registry/key.h bee/utility/unicode.h bee/lua/binding.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_socket.o : binding\lua_socket.cpp bee/lua/binding.h bee/net/socket.h bee/net/endpoint.h bee/error.h bee/net/unixsocket.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_subprocess.o : binding\lua_subprocess.cpp bee/subprocess.h bee/utility/unicode.h bee/lua/binding.h bee/error.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.

$(TMPDIR)/binding_lua_thread.o : binding\lua_thread.cpp bee/utility/semaphore.h bee/utility/lockqueue.h bee/lua/binding.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I$(LUASERIDIR) -I.

$(TMPDIR)/binding_lua_unicode.o : binding\lua_unicode.cpp bee/lua/binding.h bee/utility/unicode.h | $(TMPDIR)
	$(CXX) -c $(CFLAGS) -o $@ $<  -I$(LUADIR) -I.
