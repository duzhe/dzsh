CFLAGS=-g -Wall
dzsh: main.o list.o mempool.o cmdline_parser.o str.o
	gcc -g -Wall main.o list.o mempool.o cmdline_parser.o str.o -o dzsh
test: cmdline_parse_test
	./cmdline_parse_test
cmdline_parse_test:cmdline_parse_test.o list.o mempool.o cmdline_parser.o str.o
	gcc -g -Wall cmdline_parse_test.o list.o mempool.o cmdline_parser.o str.o -o cmdline_parse_test

main.o:main.c list.h mempool.h
list.o:list.c list.h mempool.h
mempool.o:mempool.c mempool.h list.h
cmdline_parser.o:cmdline_parser.c cmdline_parser.c mempool.h list.h
cmdline_parse_test.o:cmdline_parse_test.c cmdline_parser.c mempool.h list.h
str.o:str.c str.h mempool.h

clean:
	find . -name dzsh -a -type f -exec  rm -f dzsh \;
	find . -name '*.o' -exec rm -f {} \;

ctags:
	ctags -R
	cscope -Rbk
