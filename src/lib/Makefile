
OBJS = util.o input.o buffer.o stack.o symtab.o \
       operator.o tokenizer.o parser.o ast.o dump_ast.o \
       compiler.o bytecode.o dump_bytecode.o vm.o \
       value.o program.o c_funcs.o

libfh.a: $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.a *.o *~

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
