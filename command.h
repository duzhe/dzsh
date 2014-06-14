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

struct command {
	struct list *params;
	struct list *redirections;
	const char *bin;
};

struct command *create_command(struct mempool *pool);
BOOL command_empty(struct command *);

#endif
