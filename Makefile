ver = release

ifeq ($(ver), debug)
CFLAGS=-g -Wall -DDEBUG
else
CFLAGS=-g -Wall -O2
endif

dzsh: clean main.o list.o mempool.o cmdline_parser.o str.o ctags
	gcc -g -Wall main.o list.o mempool.o cmdline_parser.o str.o -o dzsh
debug: 
	make clean 
	CFLAGS="-g -Wall -D DEBUG" make dzsh
test: test_cmdline_parse
	./test_cmdline_parse
test_cmdline_parse:test_cmdline_parse.o list.o mempool.o cmdline_parser.o str.o
	gcc -g -Wall test_cmdline_parse.o list.o mempool.o cmdline_parser.o str.o -o test_cmdline_parse

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
