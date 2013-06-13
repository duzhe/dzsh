CFLAGS=-g -Wall
dzsh: main.o list.o mempool.o cmdline_parser.o
	gcc -g -Wall main.o list.o mempool.o cmdline_parser.o -o dzsh

main.o:main.c list.h mempool.h
list.o:list.c list.h mempool.h
mempool.o:mempool.c mempool.h list.h
cmdline_parser.o:cmdline_parser.c cmdline_parser.c mempool.h list.h

clean:
	test -f dzsh  && rm -f dzsh
	find . -name '*.o' -exec rm -f {} \;
