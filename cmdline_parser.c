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


static char *p_strdup(struct mempool *pool, const char *str)
{
	size_t len;
	char *new;
	len = strlen(str);
	new = p_alloc(pool, len +1);
	memcpy(new, str, len + 1);
	return new;
}


#define PARSE_STATE_NORMAL 0
/*
#define PARSE_STATE_QUATA 1
#define PARSE_STATE_DAUQTA 2
*/
#define PARSE_STATE_REDIRECT_TO 3
#define PARSE_STATE_REDIRECT_FROM 4

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

int cmdline_parse(struct cmdline_parser *parser, const char **pmsg)
{
	struct mempool *pool;
	struct list *process_startup_infos;
	const char *IFS;
	int state;
	char *tok, *p;
	struct redirection_pair *redirection;
	struct process_startup_info *info;

	pool = parser->pool;
	process_startup_infos = parser->process_startup_infos;
	IFS = parser->IFS;
	p = parser->p;
	tok = parser->tok;
	info = parser->info;
	state = parser->state;
	for (;*p != '\0'; ++p) {
		switch (*p) {
		case '<':
			switch (state) {
			case PARSE_STATE_NORMAL:
				*p = '\0';
				redirection = p_alloc(pool, sizeof(struct redirection_pair));
				redirection->flags = 0;
				l_pushback(info->redirections, redirection);
				if (p == tok) {
					redirection->to.fd = STDIN_FILENO;
				}
				else if (isnumeric(tok)) {
					redirection->to.fd = atoi(tok);
				}
				else {
					l_pushback(info->params, tok);
					redirection->to.fd = STDIN_FILENO;
				}
				tok = p+1;
				state = PARSE_STATE_REDIRECT_FROM;
				break;
			default:
				*pmsg = p_strdup(pool, "invalid syntax");
				return -1;
			}
			break;
		case '>':
			switch (state) {
			case PARSE_STATE_NORMAL:
				*p = '\0';
				redirection = p_alloc(pool, sizeof(struct redirection_pair));
				redirection->flags = 0;
				l_pushback(info->redirections, redirection);
				if (p == tok) {
					redirection->from.fd = STDOUT_FILENO;
				}
				else if (isnumeric(tok)) {
					redirection->from.fd = atoi(tok);
				}
				else {
					l_pushback(info->params, tok);
					redirection->from.fd = STDOUT_FILENO;
				}
				if (p[1] == '>') {
					redirection->flags |= REDIRECT_APPEND;
					p[1] = *IFS;
				}
				tok = p+1;
				state = PARSE_STATE_REDIRECT_TO;
				break;
			default:
				*pmsg = p_strdup(pool, "invalid syntax");
				return -1;
			}
			break;
		case '|':
			switch (state) {
			case PARSE_STATE_NORMAL:
				*p = '\0';
				if (tok != p) {
					l_pushback(info->params, tok);
					tok = p+1;
				}
				info = create_startup_info(pool);
				l_pushback(process_startup_infos, info);
				break;
			default:
				*pmsg = p_strdup(pool, "invalid syntax");
				return -1;
			}
			break;
		default:
			if (strchr(IFS, *p) == NULL) {
				continue;
			}
			*p = '\0';
			if (tok == p) {
				++tok;
				continue;
			}
			switch (state) {
			case PARSE_STATE_NORMAL:
				l_pushback(info->params, tok);
				tok = p+1;
				break;
			case PARSE_STATE_REDIRECT_TO:
				if (*tok == '&') {
					if (!isnumeric(tok+1)) {
						*pmsg = p_strdup(pool, "not a valid file discriptor");
						return -1;
					}
					redirection->to.fd = atoi(tok+1);
				}
				else {
					redirection->flags |= REDIRECT_TO_FILE;
					redirection->to.pathname = tok;
				}
				tok = p+1;
				state = PARSE_STATE_NORMAL;
				break;
			case PARSE_STATE_REDIRECT_FROM:
				if (*tok == '&') {
					if (!isnumeric(tok+1)) {
						*pmsg = p_strdup(pool, "not a valid file discriptor");
						return -1;
					}
					redirection->from.fd = atoi(tok+1);
				}
				else {
					redirection->flags |= REDIRECT_FROM_FILE;
					redirection->from.pathname = tok;
				}
				tok = p+1;
				state = PARSE_STATE_NORMAL;
				break;
			default:
				*pmsg = p_strdup(pool, "unexpected position");
				return -1;
			}
			break;
		}
	}
	parser->p = p;
	parser->tok = tok;
	parser->info = info;
	return 0;
}
