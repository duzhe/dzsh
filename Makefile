ver = release

ifeq ($(ver), debug)
CFLAGS=-g -Wall -DDEBUG -DPRINT_COMMAND_ONLY -DREADBUFSIZE=16
else
CFLAGS=-g -Wall
endif

dzsh: main.o list.o mempool.o parser.o str.o cmdline_buf.o env.o command.o
	gcc -g -Wall main.o list.o mempool.o parser.o str.o cmdline_buf.o env.o command.o -o dzsh
test:
	sh test.sh

cmdline_buf.o: cmdline_buf.c cmdline_buf.h bool.h
command.o: command.c command.h bool.h mempool.h list.h
env.o: env.c env.h mempool.h bool.h list.h str.h
list.o: list.c list.h bool.h mempool.h
main.o: main.c mempool.h bool.h list.h str.h parser.h cmdline_buf.h \
 command.h env.h
mempool.o: mempool.c list.h bool.h mempool.h
parser.o: parser.c parser.h bool.h cmdline_buf.h command.h mempool.h \
 list.h str.h env.h
str.o: str.c mempool.h bool.h str.h

clean:
	find . -name dzsh -a -type f -exec  rm -f {} \; || true
	find . -name '*.o' -exec rm -f {} \;

ctags:
	ctags -R
	cscope -Rbk
