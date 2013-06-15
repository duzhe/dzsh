#ifndef _CMDLINE_PARSER_H_INCLUDE
#define _CMDLINE_PARSER_H_INCLUDE

struct mempool;
struct list;
struct lnode;
struct cstr;

#define REDIRECT_FROM_FILE (0x01)
#define REDIRECT_TO_FILE (0x02)
#define REDIRECT_APPEND (0x04)

union redirection_side{
	int fd;
	struct cstr *pathname;
};

struct redirection_pair{
	int flags;
	union redirection_side from;
	union redirection_side to;
};

struct process_startup_info {
	struct list *params;
	struct list *redirections;
};

struct process_startup_info *create_startup_info(struct mempool *pool);

struct cmdline_parser;
struct cmdline_parser *create_cmdline_parser(struct mempool *pool, 
		struct list *process_startup_infos, char *cmdlinebuf,
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
int cmdline_parse(struct cmdline_parser *parser, const char **errmsg);


#endif
