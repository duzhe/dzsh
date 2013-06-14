#include "cmdline_parser.h"
#include "mempool.h"
#include "list.h"
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static int isnumeric(const char *p)
{
	while (*p !=0) {
		if (!isdigit(*p++)) {
			return 0;
		}
	}
	return 1;
}


#define PARSE_STATE_NORMAL 0
/*
#define PARSE_STATE_QUATA 1
#define PARSE_STATE_DAUQTA 2
*/

struct cmdline_parser
{
	struct mempool *pool;
	struct list *process_startup_infos;
	char *cmdlinebuf;
	const char *IFS;

	int state;
	char *p;
	char *tok;
	struct redirection_pair *redirection;
	struct process_startup_info *info;
};


struct process_startup_info *create_startup_info(struct mempool *pool)
{
	struct process_startup_info *info;
	info = p_alloc(pool, sizeof(struct process_startup_info));
	info->params = l_create(pool);
	info->redirections = l_create(pool);
	return info;
}


struct cmdline_parser *create_cmdline_parser(struct mempool *pool, 
		struct list *process_startup_infos, char *cmdlinebuf,
		const char *IFS)
{
	struct cmdline_parser *parser;
	parser = p_alloc(pool, sizeof(struct cmdline_parser));
	parser->pool = pool;
	parser->process_startup_infos = process_startup_infos;
	parser->cmdlinebuf = cmdlinebuf;
	parser->IFS = IFS;

	parser->state = PARSE_STATE_NORMAL;
	parser->p = cmdlinebuf;
	parser->tok = cmdlinebuf;
	parser->redirection = NULL;
	parser->info = create_startup_info(pool);
	l_pushback(process_startup_infos, parser->info);
	return parser;
}


/*
static char *next_token_begin(struct cmdline_parser *parser, char *p)
{
	const char *IFS;
	IFS = parser->IFS;
	while (*p != '\0' && strchr(IFS, *p) != NULL) {
		p++;
	}
	if (*p == '\0') {
		return NULL;
	}
	return p;
}
*/


static char *next_token_end(struct cmdline_parser *parser, char *p)
{
	const char *IFS = parser->IFS;
	while (*p != '\0' && strchr(IFS, *p) != NULL) {
		p++;
	}
	while (*p != '\0' && strchr(IFS, *p) == NULL) {
		p++;
	}
	if (*p == '\0') {
		return NULL;
	}
	return p;
}


int cmdline_parse_redirection(struct cmdline_parser *parser, const char **pmsg)
{
	struct mempool *pool;
	struct process_startup_info *info;
	struct redirection_pair *re;
	const char *IFS;
	char sign;
	int leftfdno;
	union redirection_side *left, *right;

	pool = parser->pool;
	info = parser->info;
	re = parser->redirection;
	if (re == NULL) {
		re = p_alloc(pool, sizeof(struct redirection_pair));
		parser->redirection = re;
		l_pushback(info->redirections, re);
	}
	re->flags = 0;
	IFS = parser->IFS;

	sign = parser->p[0];
	parser->p[0] = '\0';
	if (sign == '<') {
		left = &(re->to);
		right = &(re->from);
		leftfdno = STDIN_FILENO;
	}
	else {
		left = &(re->from);
		right = &(re->to);
		leftfdno = STDOUT_FILENO;
		if (parser->p[1] == '>') {
			re->flags |= REDIRECT_APPEND;
			parser->p[1] = *IFS;
		}
	}
	if (parser->p == parser->tok) {
		left->fd = leftfdno;
	}
	else if (isnumeric(parser->tok)) {
		left->fd = atoi(parser->tok);
	}
	else {
		l_pushback(info->params, (parser->tok));
		left->fd = leftfdno;
	}
	parser->tok = parser->p + 1;
	parser->p = next_token_end(parser, parser->p+1);
	if (parser->p == NULL) {
		*pmsg = p_strdup(pool, "invalid syntax");
		return -1;
	}
	parser->p[0] = '\0';
	if (*(parser->tok) == '&') {
		if (!isnumeric(parser->tok+1)) {
			*pmsg = p_strdup(pool, "not a valid file discriptor");
			return -1;
		}
		right->fd = atoi(parser->tok+1);
	}
	else {
		right->pathname = parser->tok;
		if (sign == '<') {
			re->flags |= REDIRECT_FROM_FILE;
		}
		else {
			re->flags |= REDIRECT_TO_FILE;
		}
	}
	re = NULL;
	parser->redirection = NULL;
	parser->tok = parser->p +1;
	return 0;
}


int cmdline_parse(struct cmdline_parser *parser, const char **pmsg)
{
	struct mempool *pool;
	struct list *process_startup_infos;
	const char *IFS;
	/*
	int state;
	*/
	struct process_startup_info *info;

	pool = parser->pool;
	process_startup_infos = parser->process_startup_infos;
	IFS = parser->IFS;
	info = parser->info;
	/*
	state = parser->state;
	*/
	for (;parser->p[0] != '\0'; parser->p += 1) {
		switch (parser->p[0]) {
		case '<':
		case '>':
			if (cmdline_parse_redirection(parser, pmsg) != 0) {
				return -1;
			}
			break;
		case '|':
			parser->p[0] = '\0';
			if (parser->tok != parser->p) {
				l_pushback(info->params, parser->tok);
			}
			parser->info = info = create_startup_info(pool);
			l_pushback(process_startup_infos, info);
			parser->tok = parser->p + 1;
			break;
		default:
			if (strchr(IFS, parser->p[0]) == NULL) {
				continue;
			}
			parser->p[0] = '\0';
			if (parser->tok == parser->p) {
				parser->tok += 1;
				continue;
			}
			l_pushback(info->params, parser->tok);
			parser->tok = parser->p + 1;
			break;
		}
	}
	return 0;
}
