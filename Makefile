
.PHONY: all clean

TEST_FILE = tests/test.fh

all: src/fh

clean:
	rm -f *~
	make -C src clean

src/fh:
	make -C src
	@echo
	@echo Compilation successful!  Try these examples:
	@echo
	@echo "  src/fh tests/test.fh"
	@echo "  src/fh tests/mandelbrot.fh"
	@echo "  src/fh tests/mandel_color.fh"
	@echo

test: src/fh
	valgrind --track-origins=yes --leak-check=full src/fh $(TEST_FILE)
