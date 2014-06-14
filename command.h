#ifndef _COMMAND_H_INCLUDE
#define _COMMAND_H_INCLUDE

#include "bool.h"

#define REDIRECT_IN (0x01)
#define REDIRECT_OUT (0x02)
#define REDIRECT_APPEND (0x04)

struct pool;
struct mempool;

union redirect_from {
	int fd;
	struct str *pathname;
};

struct redirection{
	int flags;
	int leftfd;
	union redirect_from right;
};

typedef unsigned int cmdseparator;
#define CMD_SEPARATOR_PIPE      0x01
#define CMD_SEPARATOR_LOGIC_AND 0x02
#define CMD_SEPARATOR_LOGIC_OR  0x03
#define CMD_SEPARATOR_ASNYC     0x04
#define CMD_SEPARATOR_END       0x05

struct command {
	struct list *envp;
	struct list *params;
	struct list *redirections;
	cmdseparator sep;
	const char *bin;
};

struct command *create_command(struct mempool *pool);
BOOL command_empty(struct command *);

typedef struct list* commandline;

#endif
