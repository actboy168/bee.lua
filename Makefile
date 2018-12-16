include project/make/init.mk

BINDIR = bin/$(PLAT)_$(BUILD_CONFIG)
TMPDIR = tmp/$(PLAT)_$(BUILD_CONFIG)
LUACFLAGS := $(CFLAGS)
LUALIB += -L$(BINDIR)

ifeq "$(PLAT)" "mingw"
CFLAGS += -DBEE_EXPORTS
BEE_TARGET= bee.dll
BEE_LIBS= -lws2_32 -lversion -lstdc++fs -lstdc++
else
BEE_TARGET= bee.so
BEE_LIBS=
LUALIB=
endif

default : alllua $(BINDIR)/$(BEE_TARGET)

include project/make/deps.mk
include project/make/bee.mk
include project/make/lua.mk

$(BINDIR)/$(BEE_TARGET) : $(BEE_ALL)
	$(CC) $(LDSHARED) -o $@ $^ $(LUALIB) $(BEE_LIBS)
	$(STRIP) $@

$(BINDIR) :
	mkdir -p $@

$(TMPDIR) :
	mkdir -p $@

clean :
	rm -rf $(TMPDIR)
