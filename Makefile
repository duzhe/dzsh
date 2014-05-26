ver = release

ifeq ($(ver), debug)
CFLAGS=-g -Wall -DDEBUG -DPRINT_STARTUP_INFO_ONLY -DREADBUFSIZE=16
else
CFLAGS=-g -Wall
endif

dzsh: clean main.o list.o mempool.o cmdline_parser.o str.o cmdline_buf.o ctags 
	gcc -g -Wall main.o list.o mempool.o cmdline_parser.o str.o cmdline_buf.o -o dzsh
test:
	sh test.sh

main.o:main.c list.h mempool.h
list.o:list.c list.h mempool.h
mempool.o:mempool.c mempool.h list.h
cmdline_parser.o:cmdline_parser.c cmdline_parser.c mempool.h list.h
str.o:str.c str.h mempool.h
cmdline_buf.o:cmdline_buf.c cmdline_parser.h bool.h

clean:
	find . -name dzsh -a -type f -exec  rm -f dzsh \;
	find . -name '*.o' -exec rm -f {} \;

ctags:
	ctags -R
	cscope -Rbk
