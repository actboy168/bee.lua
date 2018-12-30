ifeq "$(PLAT)" "mingw"
alllua : $(BINDIR)/lua54.dll $(BINDIR)/lua.exe $(BINDIR)/bee.exe $(BINDIR)/main.lua 
else
alllua : $(BINDIR)/lua $(BINDIR)/bee $(BINDIR)/main.lua 
ifeq "$(PLAT)" "linux"
LUALDFLAGS = -Wl,-E -lm -ldl -lreadline
else
LUALDFLAGS = -lreadline
endif
endif

LUA_FLAGS += -DLUAI_MAXCCALLS=200

CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
LUA_O=	lua.o
LUAC_O=	luac.o
BASE_O= $(CORE_O) $(LIB_O) $(MYOBJS)
ALL_O= $(BASE_O) $(LUA_O) $(LUAC_O)

define build_lua
$(TMPDIR)/lua/$(1) : $(LUADIR)/$(subst .o,.c,$(1))  | $(TMPDIR)/lua
	$(CC) -c $(LUACFLAGS) $(LUA_FLAGS) -o $(TMPDIR)/lua/$(1) $(LUADIR)/$(subst .o,.c,$(1))
endef
$(foreach v, $(ALL_O), $(eval $(call build_lua,$(v))))

$(BINDIR)/lua : $(TMPDIR)/lua/lua.o $(foreach v, $(BASE_O), $(TMPDIR)/lua/$(v)) | $(BINDIR)
	gcc -o $@ $^ $(LUALDFLAGS)
	$(STRIP) $@

$(BINDIR)/lua54.dll : $(TMPDIR)/lua/utf8_crt.o $(foreach v, $(BASE_O), $(TMPDIR)/lua/$(v)) | $(BINDIR)
	gcc -o $@ $^ $(LDSHARED)
	$(STRIP) $@

$(BINDIR)/lua.exe : $(TMPDIR)/lua/utf8_lua.o | $(BINDIR)
	gcc -o $@ $^ $(LUALIB)
	$(STRIP) $@

$(TMPDIR)/lua :
	mkdir -p $@

$(TMPDIR)/lua/utf8_crt.o : $(LUADIR)/../utf8/utf8_crt.c $(LUADIR)/../utf8/utf8_crt.h $(LUADIR)/../utf8/utf8_unicode.h  | $(TMPDIR)/lua
	$(CC) -c $(LUACFLAGS) -o $@ $<

$(TMPDIR)/lua/utf8_lua.o : $(LUADIR)/../utf8/utf8_lua.c $(LUADIR)/../utf8/utf8_unicode.h $(LUADIR)/lua.c $(LUADIR)/lprefix.h $(LUADIR)/lua.h $(LUADIR)/luaconf.h $(LUADIR)/lauxlib.h  | $(TMPDIR)/lua
	$(CC) -c $(LUACFLAGS) -o $@ $<

lapi.o: lapi.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lstring.h \
 ltable.h lundump.h lvm.h
lauxlib.o: lauxlib.c lprefix.h lua.h luaconf.h lauxlib.h
lbaselib.o: lbaselib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lcode.o: lcode.c lprefix.h lua.h luaconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lgc.h lstring.h ltable.h lvm.h
lcorolib.o: lcorolib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lctype.o: lctype.c lprefix.h lctype.h lua.h luaconf.h llimits.h
ldblib.o: ldblib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
ldebug.o: ldebug.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h lcode.h llex.h lopcodes.h lparser.h \
 ldebug.h ldo.h lfunc.h lstring.h lgc.h ltable.h lvm.h
ldo.o: ldo.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lopcodes.h \
 lparser.h lstring.h ltable.h lundump.h lvm.h
ldump.o: ldump.c lprefix.h lua.h luaconf.h lobject.h llimits.h lstate.h \
 ltm.h lzio.h lmem.h lundump.h
lfunc.o: lfunc.c lprefix.h lua.h luaconf.h lfunc.h lobject.h llimits.h \
 lgc.h lstate.h ltm.h lzio.h lmem.h
lgc.o: lgc.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
linit.o: linit.c lprefix.h lua.h luaconf.h lualib.h lauxlib.h
liolib.o: liolib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
llex.o: llex.c lprefix.h lua.h luaconf.h lctype.h llimits.h ldebug.h \
 lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lgc.h llex.h lparser.h \
 lstring.h ltable.h
lmathlib.o: lmathlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lmem.o: lmem.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h
loadlib.o: loadlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lobject.o: lobject.c lprefix.h lua.h luaconf.h lctype.h llimits.h \
 ldebug.h lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h \
 lvm.h
lopcodes.o: lopcodes.c lprefix.h lopcodes.h llimits.h lua.h luaconf.h
loslib.o: loslib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lparser.o: lparser.c lprefix.h lua.h luaconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lfunc.h lstring.h lgc.h ltable.h
lstate.o: lstate.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h llex.h \
 lstring.h ltable.h
lstring.o: lstring.c lprefix.h lua.h luaconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h
lstrlib.o: lstrlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
ltable.o: ltable.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
ltablib.o: ltablib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
ltm.o: ltm.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
lua.o: lua.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
luac.o: luac.c lprefix.h lua.h luaconf.h lauxlib.h lobject.h llimits.h \
 lstate.h ltm.h lzio.h lmem.h lundump.h ldebug.h lopcodes.h
lundump.o: lundump.c lprefix.h lua.h luaconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h \
 lundump.h
lutf8lib.o: lutf8lib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
lvm.o: lvm.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h \
 ltable.h lvm.h ljumptab.h
lzio.o: lzio.c lprefix.h lua.h luaconf.h llimits.h lmem.h lstate.h \
 lobject.h ltm.h lzio.h

$(TMPDIR)/bootstrap/main.o : bootstrap/main.cpp | $(TMPDIR)/bootstrap
	$(CXX) -c $(CFLAGS) -o $@ $< -I$(LUADIR)

$(TMPDIR)/bootstrap/progdir.o : bootstrap/progdir.cpp | $(TMPDIR)/bootstrap
	$(CXX) -c $(CFLAGS) -o $@ $< -I$(LUADIR)

$(BINDIR)/bee : $(TMPDIR)/bootstrap/main.o $(TMPDIR)/bootstrap/progdir.o $(foreach v, $(BASE_O), $(TMPDIR)/lua/$(v)) | $(BINDIR)
	gcc -o $@ $^ $(LUALDFLAGS)
	$(STRIP) $@

$(BINDIR)/bee.exe : $(TMPDIR)/bootstrap/main.o $(TMPDIR)/bootstrap/progdir.o | $(BINDIR)
	gcc -o $@ $^ $(LUALIB)
	$(STRIP) $@

$(BINDIR)/main.lua : bootstrap/main.lua
	cp $^ $@

$(TMPDIR)/bootstrap :
	mkdir -p $@
