include project/make/init.mk

BINDIR = bin/$(PLAT)_$(BUILD_CONFIG)
TMPDIR = tmp/$(PLAT)_$(BUILD_CONFIG)
LUACFLAGS := $(CFLAGS)
CFLAGS += -DBEE_EXPORTS

default : alllua $(BINDIR)/bee.dll

include project/make/deps.mk
include project/make/bee.mk
include project/make/lua.mk

$(BINDIR)/bee.dll : $(BEE_ALL)
	$(CC) $(LDSHARED) $(CFLAGS) -o $@ $^ $(LUALIB) -lstdc++fs -lstdc++ -lws2_32 -lversion
	$(STRIP) $@

$(BINDIR) :
	mkdir -p $@

$(TMPDIR) :
	mkdir -p $@

clean :
	rm -rf $(TMPDIR)
