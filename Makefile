SRC=odielak.c
TARGET=odielak.so
TESTFILE=test.lua

ifdef LUAPKG
	PREFIX=$(shell pkg-config --variable=prefix $(LUAPKG))
	LUA_LIBDIR=$(shell pkg-config --variable=INSTALL_CMOD $(LUAPKG))
	LUA_INCDIR=$(shell pkg-config --cflags $(LUAPKG))
	LUA=$(LUAPKG)
else
	PREFIX?=/usr/local
	LUA_LIBDIR?=$(PREFIX)/lib/lua/5.1
	LUA_INCDIR?=-I$(PREFIX)/include/luajit-2.0
	LUA?=$(PREFIX)/bin/luajit
endif

CCOPT=-O3 -Wall -fPIC $(CFLAGS) $(LUA_INCDIR)
LDOPT=-shared $(LDFLAGS)
OBJ=$(SRC:.c=.o)
CHMOD=755

ifeq ($(CC), clang)
	LDOPT+= -undefined dynamic_lookup
endif

all: build

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDOPT) $(OBJ) -o $@

.c.o:
	$(CC) -c $(CCOPT) $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

install: $(TARGET)
	mkdir -p $(LUA_LIBDIR)
	cp $(TARGET) $(LUA_LIBDIR)/$(TARGET)
	chmod $(CHMOD) $(LUA_LIBDIR)/$(TARGET)

test: $(TARGET)
	$(LUA) $(TESTFILE)

.PHONY: all build clean install test
