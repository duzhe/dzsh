#ifndef _PARSER_H_INCLUDE
#define _PARSER_H_INCLUDE
#include <stdio.h>
#include "bool.h"
#include "cmdline_buf.h"
#include "command.h"

struct mempool;
struct list;
struct env;
struct parser;

struct parser *create_parser(struct mempool *pool, 
		struct list *cmdlist, struct cmdline_buf  *buf,
		struct env *env);

#define CMDLINE_PARSE_DONE 0x00
#define CMDLINE_PARSE_OK 0x01
#define CMDLINE_PARSE_CONTINUE 0x02
#define CMDLINE_PARSE_SYNTAX_ERROR 0x03
#define CMDLINE_PARSE_EMPTY 0x04
#define CMDLINE_READING_TERMINATE 0x05
#define CMDLINE_READING_ERROR 0x06
#define CMDLINE_PARSE_TOKEN_TOO_LONG 0x07
/* OK is an internal state, parser_parse will return DONE, CONTINUE
 * or SYNTAX_ERROR ONLY */
int parser_parse(struct parser *parser);
const char *errmsg(struct parser *parser);


#endif
