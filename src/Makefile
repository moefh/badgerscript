
OBJS = main.o functions.o

EXTRA_CFLAGS = -Ilib

.PHONY: $(TARGETS) clean

fh: $(OBJS) lib/libfh.a
	$(CC) $(LDFLAGS) -o $@ $(OBJS) lib/libfh.a $(LIBS)

lib/libfh.a:
	$(MAKE) -C lib

clean:
	rm -f fh *.o *~
	$(MAKE) -C lib clean

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<

.c.o:
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<
