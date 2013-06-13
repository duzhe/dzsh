#ifndef _CMDLINE_PARSER_H_INCLUDE
#define _CMDLINE_PARSER_H_INCLUDE

struct mempool;
struct list;
struct lnode;

#define REDIRECT_FROM_FILE (0x01)
#define REDIRECT_TO_FILE (0x02)
#define REDIRECT_APPEND (0x04)
struct redirection_pair{
	int flags;
	union {
		int fd;
		char *pathname;
	} from;
	union {
		int fd;
		char *pathname;
	} to;
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
int cmdline_parse(struct cmdline_parser *parser, const char **errmsg);


#endif
