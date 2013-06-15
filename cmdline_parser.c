#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "cmdline_parser.h"
#include "mempool.h"
#include "list.h"
#include "str.h"

/*
static int isnumeric(const char *p)
{
	while (*p !=0) {
		if (!isdigit(*p++)) {
			return 0;
		}
	}
	return 1;
}
*/


#define PARSE_STATE_NORMAL 0
#define PARSE_STATE_QUATA 1
#define PARSE_STATE_DAUQTA 2

struct cmdline_parser
{
	struct mempool *pool;
	struct list *process_startup_infos;
	const char *cmdlinebuf;
	const char *IFS;

	int state;
	const char *p;
	const char *tok;
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


/*
static const char *next_token_end(struct cmdline_parser *parser, const char *p)
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
*/


static int cmdline_next_tok(struct cmdline_parser *parser, const char *begin, 
		struct cstr **tok, const char **pend)
{
	struct mempool *pool;
	const char *IFS;
	const char *tokend;

	pool = parser->pool;
	IFS = parser->IFS;

	for (; *begin != '\0' && strchr(IFS, *begin) != NULL; ++begin) {
	}
	if (*begin == '\0') {
		return CMDLINE_PARSE_SYNTAX_ERROR;
	}
	for (tokend=begin; *tokend != '\0' && strchr(IFS, *tokend) == NULL; ++tokend) {
	}
	if (pend != NULL) {
		*pend = tokend +1;
	}
	*tok= make_cstr(pool, begin, tokend-begin);
	return CMDLINE_PARSE_OK;
}

int cmdline_parse_redirection(struct cmdline_parser *parser, const char **pmsg)
{
	struct mempool *pool;
	struct process_startup_info *info;
	struct redirection_pair re;
	struct redirection_pair *pre;
	char sign;
	const char *tok, *p;
	int leftdeffd;
	union redirection_side *left, *right;
	struct cstr *str;
	int retval;

	pool = parser->pool;
	info = parser->info;
	tok = parser->tok;
	p = parser->p;

	re.flags = 0;
	sign = *p;

	if (sign == '<') {
		left = &(re.to);
		right = &(re.from);
		leftdeffd = STDIN_FILENO;
	}
	else {
		left = &(re.from);
		right = &(re.to);
		leftdeffd = STDOUT_FILENO;
	}
	if (p == tok) {
		left->fd = leftdeffd;
	}
	else { 
		str = make_cstr(pool, tok, p - tok);
		if (cstr_isnumeric(str)) {
			left->fd = cstr_atoi(str);
		}
		else {
			l_pushback(info->params, str);
			left->fd = leftdeffd;
			parser->tok = parser->p;
		}
	}
	if (sign == '>' && p[1] == '>') {
		re.flags |= REDIRECT_APPEND;
		p += 1;
	}
	tok = p+1;
	retval = cmdline_next_tok(parser, tok, &str, &p);
	if (retval != CMDLINE_PARSE_OK) {
		return retval;
	}
	if (*(str->data) == '&') {
		str->data += 1;
		str->len -= 1;
		if (!cstr_isnumeric(str)) {
			*pmsg = p_strdup(pool, "not a valid file discriptor");
			return CMDLINE_PARSE_SYNTAX_ERROR;
		}
		right->fd = cstr_atoi(str);
	}
	else {
		right->pathname = str;
		if (sign == '<') {
			re.flags |= REDIRECT_FROM_FILE;
		}
		else {
			re.flags |= REDIRECT_TO_FILE;
		}
	}
	pre = p_alloc(pool, sizeof(struct redirection_pair));
	memcpy(pre, &re, sizeof(struct redirection_pair));
	l_pushback(info->redirections, pre);
	parser->p = p;
	parser->tok = p;
	return CMDLINE_PARSE_OK;
}


int cmdline_parse(struct cmdline_parser *parser, const char **pmsg)
{
	int result;
	struct mempool *pool;
	struct list *process_startup_infos;
	const char *IFS;
	const char *tok, *p;
	struct cstr *str;
	/*
	int state;
	*/
	struct process_startup_info *info;

	pool = parser->pool;
	process_startup_infos = parser->process_startup_infos;
	IFS = parser->IFS;
	info = parser->info;
	tok = parser->tok;
	p = parser->p;
	/*
	state = parser->state;
	*/
	for (;*p != '\0'; ++p) {
		switch (*p) {
		case '<':
		case '>':
			parser->p = p;
			parser->tok = tok;
			result = cmdline_parse_redirection(parser, pmsg);
			p = parser->p;
			tok = parser->tok;
			if (result != CMDLINE_PARSE_OK) {
				return result;
			}
			break;
		case '|':
			if (tok != p) {
				str = make_cstr(pool, tok, p-tok);
				l_pushback(info->params, str);
			}
			parser->info = info = create_startup_info(pool);
			l_pushback(process_startup_infos, info);
			parser->tok = tok = p + 1;
			break;
		default:
			if (strchr(IFS, *p) == NULL) {
				continue;
			}
			if (tok == p) {
				parser->tok = tok = p + 1;
				continue;
			}
			str = make_cstr(pool, tok, p - tok);
			l_pushback(info->params,  str);
			parser->tok = tok = p + 1;
			break;
		}
	}
	parser->p = p;
	return CMDLINE_PARSE_DONE;
}
