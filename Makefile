CFLAGS=-g -Wall
dzsh: main.o list.o mempool.o
	gcc -g -Wall main.o list.o mempool.o -o dzsh

main.o:main.c
list.o:list.c list.h mempool.h
mempool.o:mempool.c mempool.h list.h

clean:
	test -f dzsh  && rm -f dzsh
	ls *.o > /dev/null 2>dev/null && rm -f *.o
