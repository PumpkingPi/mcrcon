# if you want to cross compile:
#   export PATH=$PATH:/path/to/compiler/bin
#   export CROSS_COMPILE=arm-none-linux-gnueabi-
#   make
#
# Windows cross compile:
#   x86_64-w64-mingw32-gcc -std=gnu99 -Wall -Wextra -Wpedantic -O2 -fstack-protector-all -o mcrcon.exe mcrcon.c -lws2_32

EXENAME = mcrcon
PREFIX ?= /usr/local

INSTALL = install
LINKER =
RM = rm -v -f

CC ?= gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Wpedantic -O2
EXTRAFLAGS ?= -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -pie

ifeq ($(OS), Windows_NT)
	LINKER = -lws2_32
	EXENAME = mcrcon.exe
	RM = cmd /C del /F
endif

.PHONY: all
all: $(EXENAME)

$(EXENAME): mcrcon.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(EXTRAFLAGS) -o $@ $< $(LINKER)

ifneq ($(OS), Windows_NT)
.PHONY: install
install:
	$(INSTALL) -v $(EXENAME) $(DESTDIR)$(PREFIX)/bin/$(EXENAME)
	$(INSTALL) -v -m 0644 mcrcon.1 $(DESTDIR)$(PREFIX)/share/man/man1/mcrcon.1
	@echo "\nmcrcon installed. Run 'make uninstall' if you want to uninstall.\n"

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(EXENAME) $(DESTDIR)$(PREFIX)/share/man/man1/mcrcon.1
	@echo "\nmcrcon uninstalled.\n"
endif

.PHONY: clean
clean:
	$(RM) $(EXENAME) $(EXENAME).exe
