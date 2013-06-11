CFLAGS=-g -Wall
dzsh: main.o
	gcc -g -Wall main.o -o dzsh

main.o:main.c

clean:
	test -f dzsh  && rm -f dzsh
	test -f *.o  && rm -f *.o
