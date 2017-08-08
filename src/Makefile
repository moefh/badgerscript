
CC = gcc
#CFLAGS = -O2 -Wall -Wextra -g -Ilib/
CFLAGS = -O0 -Wall -Wextra -g -Ilib/
LDFLAGS =

OBJS = main.o
LIBS = lib/libfh.a -lm

.PHONY: all lib clean

all: fh

fh: $(OBJS) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

lib/libfh.a:
	make -C lib

clean:
	rm -f fh *.o *~
	make -C lib clean

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
