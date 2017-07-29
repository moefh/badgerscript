
.PHONY: all clean

all: src/fh

clean:
	rm -f *~
	make -C src clean

src/fh:
	make -C src

test: src/fh
	valgrind --track-origins=yes --leak-check=full src/fh tests/test.fh
