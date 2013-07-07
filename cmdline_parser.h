#ifndef _CMDLINE_PARSER_H_INCLUDE
#define _CMDLINE_PARSER_H_INCLUDE
#include "bool.h"

struct mempool;
struct list;
struct lnode;
struct cstr;

#define REDIRECT_IN (0x01)
#define REDIRECT_FILE (0x02)
#define REDIRECT_APPEND (0x04)

/*
struct cmdlist 
{
	struct list *command;
};
*/

union redirection_side {
	int fd;
	struct cstr *pathname;
};

struct redirection{
	int flags;
	int leftfd;
	union redirection_side right;
};

struct process_startup_info {
	struct list *params;
	struct list *redirections;
	const char *bin;
};

BOOL startupinfo_empty(struct process_startup_info *);

struct process_startup_info *create_startup_info(struct mempool *pool);

struct cmdline_parser;
struct cmdline_parser *create_cmdline_parser(struct mempool *pool, 
		struct list *cmdlist, char *cmdlinebuf,
		const char *IFS);

#define CMDLINE_PARSE_DONE 0x00
#define CMDLINE_PARSE_OK 0x01
#define CMDLINE_PARSE_CONTINUE 0x02
#define CMDLINE_PARSE_SYNTAX_ERROR 0x03
#define CMDLINE_PARSE_EMPTY 0x04
#define CMDLINE_READING_TERMINATE 0x05
#define CMDLINE_READING_ERROR 0x06
/* OK is an internal state, cmdline_parse will return DONE, CONTINUE
 * or SYNTAX_ERROR ONLY */
int cmdline_parse(struct cmdline_parser *parser);
const char *errmsg(struct cmdline_parser *parser);


#endif
