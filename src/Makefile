
CC = gcc
CFLAGS = -O2 -Wall -g
LDFLAGS =

OBJS = main.o
LIBS = lib/libfh.a

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
