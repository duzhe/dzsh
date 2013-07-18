#ifndef _CMDLINE_PARSER_H_INCLUDE
#define _CMDLINE_PARSER_H_INCLUDE
#include <stdio.h>
#include "bool.h"

struct mempool;
struct list;
struct lnode;
struct cstr;

#define REDIRECT_IN (0x01)
#define REDIRECT_FILE (0x02)
#define REDIRECT_APPEND (0x04)

#ifndef READBUFSIZE 
#define READBUFSIZE 8192
#endif

#if READBUFSIZE < 16
#define READBUF 16
#endif

/*
struct cmdlist 
{
	struct list *command;
};
*/

struct cmdline_buf {
	char data[READBUFSIZE];
	char *p;
	size_t space;
};

void cmdline_buf_clear(struct cmdline_buf *);
const char *cmdline_buf_getline(struct cmdline_buf *, FILE* infile);
void cmdline_buf_parsed(struct cmdline_buf *, const char *parsed);
BOOL cmdline_buf_line_complete(struct cmdline_buf *);

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
		struct list *cmdlist, struct cmdline_buf  *buf,
		const char *IFS);

#define CMDLINE_PARSE_DONE 0x00
#define CMDLINE_PARSE_OK 0x01
#define CMDLINE_PARSE_CONTINUE 0x02
#define CMDLINE_PARSE_SYNTAX_ERROR 0x03
#define CMDLINE_PARSE_EMPTY 0x04
#define CMDLINE_READING_TERMINATE 0x05
#define CMDLINE_READING_ERROR 0x06
#define CMDLINE_PARSE_TOKEN_TOO_LONG 0x07
/* OK is an internal state, cmdline_parse will return DONE, CONTINUE
 * or SYNTAX_ERROR ONLY */
int cmdline_parse(struct cmdline_parser *parser);
const char *errmsg(struct cmdline_parser *parser);


#endif
