include project/make/init.mk

BINDIR = bin/$(PLAT)_$(BUILD_CONFIG)
TMPDIR = tmp/$(PLAT)_$(BUILD_CONFIG)
CFLAGS += -DBEE_EXPORTS

default : $(BINDIR)/bee.dll

include project/make/deps.mk
include project/make/bee.mk

lua : | $(BINDIR)
	@cd $(LUADIR) && $(MAKE) --no-print-directory "PLAT=$(PLAT)" "CC=$(CC)" "CFLAGS=$(CFLAGS) $(SYSCFLAGS)"
	@cp $(LUADIR)/liblua.a  $(BINDIR)
	@cp $(LUADIR)/lua.exe   $(BINDIR)
	@cp $(LUADIR)/lua54.dll $(BINDIR)

$(BINDIR)/bee.dll : $(BEE_ALL) | lua
	$(CC) $(LDSHARED) $(CFLAGS) -o $@ $^ $(LUALIB) -lstdc++fs -lstdc++ -lws2_32 -lversion
	$(STRIP) $@

$(BINDIR) :
	mkdir -p $@

$(TMPDIR) :
	mkdir -p $@

clean :
	cd $(LUADIR) && $(MAKE) clean
	rm -rf $(TMPDIR)
