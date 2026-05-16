CC       = gcc

# Fedora uses 'lua', Debian/Ubuntu uses 'lua5.4'
LUA_PC   := $(shell pkg-config --exists lua && echo lua || echo lua5.4)
CFLAGS   = -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE $(shell pkg-config --cflags $(LUA_PC))
LDFLAGS  = -lreadline $(shell pkg-config --libs $(LUA_PC)) -lm

PREFIX   ?= /usr/local
BINDIR   = $(PREFIX)/bin
DATADIR  = $(PREFIX)/share/sitool

# Bake system handler path into the binary
CFLAGS  += -DSITOOL_DATADIR=\"$(DATADIR)\"

SRC      = src/main.c src/serial.c src/sitool.c src/handler.c src/mode.c src/utils.c
OBJ      = $(SRC:.c=.o)
BIN      = sitool

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -d $(DESTDIR)$(DATADIR)/handlers
	cp -r handlers/* $(DESTDIR)$(DATADIR)/handlers/ 2>/dev/null || true

.PHONY: all clean install
