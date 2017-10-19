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

typedef unsigned int cmdseperator;
#define CMD_SEPERATOR_NONE      0x00
#define CMD_SEPERATOR_PIPE      0x01
#define CMD_SEPERATOR_LOGIC_AND 0x02
#define CMD_SEPERATOR_LOGIC_OR  0x03
#define CMD_SEPERATOR_ASNYC     0x04
#define CMD_SEPERATOR_END       0x05

typedef unsigned int cmdtype;

#define CMD_TYPE_SIMPLE 0x01
#define CMD_TYPE_PIPE   0x02
#define CMD_TYPE_LOGIC  0x03
#define CMD_TYPE_SEMICOLON 0x04


struct command {
	cmdtype type;
};

struct simple_command {
	cmdtype type;
	struct list *envp;
	struct list *params;
	struct list *redirections;
	cmdseperator sep;
	const char *bin;
};

struct complex_command {
	cmdtype type;
	struct list *commands;
};

struct simple_command *create_command(struct mempool *pool);
struct complex_command *create_pipe_command(struct mempool *pool);
struct complex_command *create_logic_command(struct mempool *pool);

BOOL command_empty(struct simple_command *);
cmdseperator command_get_seperator(struct command *cmd);

#endif
