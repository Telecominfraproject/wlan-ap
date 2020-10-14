#/* SPDX-License-Identifier: BSD-3-Clause */
CC=gcc
WFLAGS=-Wall
CFLAGS=-O2
INCLUDES=-Iinclude
LIBS= -lssl
LIBNAME=libwebsocket.so

CFLAGS += -O -Wall -Werror -Wshadow -fPIC

all: $(LIBNAME)

%.o: %.c
	$(CC) $(WFLAGS) -c -o $@ $(INCLUDES) $(CFLAGS) $<

OBJS=alloc.o base64-decode.o client.o client-handshake.o client-parser.o context.o getifaddrs.o handshake.o header.o libwebsockets.o lws-plat-unix.o output.o parsers.o pollfd.o server.o server-handshake.o service.o sha-1.o ssl.o ssl-client.o ssl-server.o

$(LIBNAME): $(OBJS)
	$(CC) $(CFLAGS) -Wl,-Bsymbolic-functions -shared -o $@ $^ $(LIBS)

clean:
	$(RM) -f libwebsocket.so $(OBJS)
