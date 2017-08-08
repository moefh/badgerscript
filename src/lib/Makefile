
CC = gcc
CFLAGS = -O2 -Wall -Wextra -g
#CFLAGS = -O0 -Wall -Wextra -g

AR = ar
ARFLAGS = rs

OBJS = util.o input.o buffer.o stack.o symtab.o \
       operator.o tokenizer.o parser.o ast.o dump_ast.o \
       compiler.o bytecode.o dump_bytecode.o vm.o \
       c_funcs.o fh.o

.PHONY: all clean

all: libfh.a

libfh.a: $(OBJS)
	$(AR) $(ARFLAGS) libfh.a $(OBJS)

clean:
	rm -f *.a *.o *~

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
