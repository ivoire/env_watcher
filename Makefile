RM      = rm
INSTALL = install
PREFIX  = /usr/local
CC      = gcc
CFLAGS  = -Wall -O2
LDFLAGS = -ldl -lrt -lm -fPIC


env_watcher.so: env_watcher.c Makefile
	$(CC) $(CFLAGS) env_watcher.c -o env_watcher.so -shared $(LDFLAGS)

clean:
	$(RM) -f env_watcher.so

install: env_watcher.so
	$(INSTALL) -d $(PREFIX)/lib
	$(INSTALL) env_watcher.so $(PREFIX)/lib

uninstall:
	$(RM) $(PREFIX)/lib/env_watcher.so

.PHONY: clean install uninstall
