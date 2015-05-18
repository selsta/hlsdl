# Makefile inspiration from Kore (Kore.io)
CC?=gcc
PREFIX?=/usr/local
HLSDL=hlsdl
INSTALL_DIR=$(PREFIX)/bin

S_SRC= src/main.c src/aes.c src/curl.c src/hls.c src/misc.c src/msg.c	
S_OBJS=	$(S_SRC:.c=.o)

CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=-Wsign-compare -Iincludes -g
CFLAGS+=-DPREFIX='"$(PREFIX)"'
LDFLAGS+=-lcurl -lavformat -lavutil -lavcodec

OSNAME=$(shell uname -s | sed -e 's/[-_].*//g' | tr A-Z a-z)
ifeq ("$(OSNAME)", "darwin")
	CFLAGS+=-I/usr/local/include/
	LDFLAGS+=-L/usr/local/lib
else ifeq ("$(OSNAME)", "linux")
	CFLAGS+=-D_GNU_SOURCE=1 -std=gnu99
else
endif

all: $(S_OBJS)
	$(CC) $(S_OBJS) $(LDFLAGS) -o $(HLSDL)

install:
	mkdir -p $(INSTALL_DIR)
	install -m 555 $(HLSDL) $(INSTALL_DIR)/$(HLSDL)

uninstall:
	rm -f $(INSTALL_DIR)/$(HLSDL)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	find . -type f -name \*.o -exec rm {} \;
	rm -f $(HLSDL)

.PHONY: clean
