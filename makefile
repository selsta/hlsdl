# Makefile inspiration from Kore (Kore.io)
CC?=gcc
PREFIX?=/usr/local
HLSDL=hlsdl
INSTALL_DIR=$(PREFIX)/bin
OSNAME=$(shell uname -s | sed -e 's/[-_].*//g' | tr A-Z a-z)

S_SRC= src/main.c src/aes_openssl.c src/curl.c src/hls.c src/misc.c src/msg.c src/mpegts.c	
ifeq ("$(OSNAME)", "darwin")
	CFLAGS+=-I/usr/local/include/
	LDFLAGS+=-L/usr/local/lib
else ifeq ("$(OSNAME)", "linux")
	CFLAGS+=-D_GNU_SOURCE=1 -std=gnu99
else ifeq ("$(OSNAME)", "mingw32")
	CFLAGS+=-D_GNU_SOURCE=1 -std=gnu99 -DCURL_STATICLIB
	S_SRC+=msvc/win/memmem.c
else
endif
S_OBJS=	$(S_SRC:.c=.o)

CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=-Wsign-compare -Iincludes
CFLAGS+=-DPREFIX='"$(PREFIX)"'

ifeq ("$(OSNAME)", "mingw32")
	LDFLAGS+=-Wl,-Bstatic -lpthread -lcurl  -lssl -lcrypto -lwsock32 -lws2_32 -lwldap32 -lz
else
	LDFLAGS+=-lpthread -lcurl -lcrypto
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
