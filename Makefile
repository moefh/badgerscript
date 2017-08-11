
CC = gcc
AR = ar rc
RANLIB = ranlib
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS =
LIBS = -lm

CHECK_SCRIPT = tests/test.fh

TARGETS = debug release ubsan

.PHONY: $(TARGETS) build clean check test dump_exported_symbols

all: debug

debug:
	$(MAKE) build TARGET_CFLAGS="-g" TARGET_LDFLAGS=""

release:
	$(MAKE) build TARGET_CFLAGS="-O2" TARGET_LDFLAGS="-s"

ubsan:
	$(MAKE) build TARGET_CFLAGS="-g -fsanitize=undefined" TARGET_LDFLAGS="-fsanitize=undefined"

clean:
	rm -f *~
	$(MAKE) -C src clean

build:
	$(MAKE) -C src CFLAGS="$(CFLAGS) $(TARGET_CFLAGS)" CC="$(CC)" LDFLAGS="$(LDFLAGS) $(TARGET_LDFLAGS)" LIBS="$(LIBS)" AR="$(AR)" RANLIB="$(RANLIB)"
	@echo
	@echo "Compilation successful!  Try these examples:"
	@echo
	@echo "  src/fh tests/test.fh"
	@echo "  src/fh tests/mandelbrot.fh"
	@echo "  src/fh tests/mandel_color.fh"
	@echo

check: debug
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all src/fh $(CHECK_SCRIPT) arg1 arg2

test: debug
	src/fh tests/mandel_color.fh

dump_exported_symbols: debug
	nm src/lib/libfh.a | grep " [A-TV-Zuvw] "
