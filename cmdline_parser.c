#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "cmdline_parser.h"
#include "mempool.h"
#include "list.h"
#include "str.h"
#include <stdio.h>

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

struct tokenmap {
	int nextstate;
	int offset;
};

/*
static struct tokenmap map_token_begin[256];
*/

#define PARSE_STATE_BEGIN		0x02
#define PARSE_STATE_NORMAL 		0x03
#define PARSE_STATE_DONE		0x04
#define PARSE_STATE_SQUOTE 		0x05
#define PARSE_STATE_DQUOTE 		0x06
#define PARSE_STATE_FUNCTION 	0x07
#define PARSE_STATE_BACKTICK 	0x08
/*
#define PARSE_STATE_ENCLOSE		0x06
*/
#define PARSE_STATE_BACKSLASH 	0x09
#define PARSE_STATE_REDIRECT	0x10
#define PARSE_STATE_ASSIGMENT	0x11
#define PARSE_STATE_TEST		0x12
#define PARSE_STATE_COMMENT		0x13
#define PARSE_STATE_JOB			0x14
#define PARSE_STATE_FUNCBODY	0x15

#define PARSE_PHASE_TOKEN	0x00
#define PARSE_PHASE_ESCAPE  0x01
#define PARSE_PHASE_COUNT   0x02


struct cmdline_parser
{
	struct mempool *pool;
	struct list *cmdlist;
	const char *cmdlinebuf;
	const char *IFS;

	int phase;
	int state;
	const char *tokbegin;
	const char *p;
	struct process_startup_info *info;
	struct list *toklist;
	const char *errmsg;
};

typedef int (*cmdline_parse_phase_func)(struct cmdline_parser *);
static int cmdline_parse_token(struct cmdline_parser *parser);
static int cmdline_parse_classication(struct cmdline_parser *parser);
#ifdef DEBUG
static int cmdline_parse_expand(struct cmdline_parser *parser);
static int cmdline_parse_token_print(struct cmdline_parser *parser);
static int cmdline_parse_end(struct cmdline_parser *parser);
#endif

static cmdline_parse_phase_func phase_func[] = {
	&cmdline_parse_token,
#ifdef DEBUG
	&cmdline_parse_token_print,
#endif
	&cmdline_parse_classication,
	/*
	&cmdline_parse_end,
	&cmdline_parse_expand,
	*/
};


#define TOKEN_TYPE_NORMAL 				0x01
#define TOKEN_TYPE_REDIRECT				0x02
#define TOKEN_TYPE_PIPE					0x03
#define TOKEN_TYPE_AND					0x03
#define TOKEN_TYPE_DAND					0x04
#define TOKEN_TYPE_SIMICOLON			0x05
#define TOKEN_TYPE_ENDLINE				0x06
#define TOKEN_TYPE_TERMINATE			0x07
#define TOKEN_TYPE_COMMENT				0x08
#define TOKEN_TYPE_TEST					0x09
#define TOKEN_TYPE_JOB					0x09
#define TOKEN_TYPE_FUNCNAME				0x0a
#define TOKEN_TYPE_FUNCBODY				0x0b
/*
#define TOKEN_TYPE_SQUOTE				0x0c
#define TOKEN_TYPE_DQUOTE				0x0d
*/
#define TOKEN_TYPE_BACKTICK				0x0e

#define TOKEN_FLAGS_VARIABLE			0x01
#define TOKEN_FLAGS_SPECIALVAR			0x02
#define TOKEN_FLAGS_WILDCARD			0x04
#define TOKEN_FLAGS_INLINE_SUBPROCESS	0x08
#define TOKEN_FLAGS_DQUOTED				0x0F
#define TOKEN_FLAGS_SQUOTED				0x10
#define TOKEN_FLAGS_BACKSLASH			0x20

struct token {
	unsigned int type:16;
	unsigned int flags:16;
	struct cstr tok;
};


struct process_startup_info *create_startup_info(struct mempool *pool)
{
	struct process_startup_info *info;
	info = p_alloc(pool, sizeof(struct process_startup_info));
	info->params = l_create(pool);
	info->redirections = l_create(pool);
	info->bin = NULL;
	return info;
}


struct cmdline_parser *create_cmdline_parser(struct mempool *pool, 
		struct list *cmdlist, char *cmdlinebuf,
		const char *IFS)
{
	struct cmdline_parser *parser;
	parser = p_alloc(pool, sizeof(struct cmdline_parser));
	parser->pool = pool;
	parser->cmdlist = cmdlist;
	parser->cmdlinebuf = cmdlinebuf;
	parser->IFS = IFS;
	parser->toklist = l_create(pool);
	parser->phase = PARSE_PHASE_TOKEN;
	parser->state = PARSE_STATE_BEGIN;
	parser->p = cmdlinebuf;
	parser->tokbegin = cmdlinebuf;
	return parser;
}

BOOL startupinfo_empty(struct process_startup_info *info)
{
	return l_empty(info->params) && l_empty(info->redirections);
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


static const char *get_enclose(const char *begin, int ch)
{
	const char *p;
	p = begin;
	while (*p != '\0') {
		if (*p == ch) {
			return p;
		}
		if (*p == '\\') {
			p++;
			if (*p == '\0') {
				break;
			}
		}
		p++;
	}
	return p;
}


static const char *get_function_end(const char *p)
{
	int state;
	int count;
	state = 0;
	count = 0;
	for (;*p != '\0';++p) {
		switch (*p) {
		case '\\':
			/* skip next charactor whatever it is */
			++p;
			if (*p == '\0') {
				return p;
			}
			break;
		case '\'':
		case '"':
		case '`':
			if (state == 0) {
				state = *p;
			}
			else if (state == *p) {
				state = 0;
			}
			break;
		case '{':
			if (state == 0) {
				++count;
			}
			break;
		case '}':
			if (state == 0) {
				--count;
			}
			if (count <= 0) {
				return p;
			}
			break;
		default:
			break;
		}
	}
	return p;
}


const char *get_token_begin(const char *begin, const char *IFS)
{
	for (; begin && *begin != '\0' && *begin != '\n' && strchr(IFS, *begin) != NULL; ++begin);
	return begin;
}


const char *get_token_end(const char *begin, const char *IFS)
{
	for (; begin && *begin != '\0' && strchr(IFS, *begin) == NULL; ++begin);
	return begin;
}


int cmdline_parse_redirection(struct cmdline_parser *parser, struct cstr *token, 
		struct redirection *re)
{
	struct mempool *pool;
	const char *p;
	struct cstr t;
	int leftdeffd;
	const char *IFS;
	
	pool = parser->pool;
	IFS = parser->IFS;
	re->flags = 0;
	for (p = token->data; *p != '<' && *p != '>'; ++p);
	if (*p == '<') {
		leftdeffd = STDIN_FILENO;
		re->flags |= REDIRECT_IN;
	}
	else {
		leftdeffd = STDOUT_FILENO;
	}
	if (p == token->data) {
		re->leftfd = leftdeffd;
	}
	else {
		t.data = token->data;
		t.len = p - token->data;
		re->leftfd = cstr_atoi(&t);
	}
	if (*p == '>' && *(p+1) == '>') {
		re->flags |= REDIRECT_APPEND;
		p+=2;
	}
	else {
		p+=1;
	}
	if (*p == '&') {
		t.data = p+1;
		t.len = token->data + token->len - (p+1);
		re->right.fd = cstr_atoi(&t);
	}
	else {
		/*
		p = get_token_begin(p, IFS);
		if (p == NULL || *p == '\n') {
			parser->errmsg = "unexpected newline";
			return CMDLINE_PARSE_SYNTAX_ERROR;
		}
		re->right.pathname = make_cstr(pool, p, token->data + token->len - p);
		*/
		re->flags |= REDIRECT_FILE;
	}
	return CMDLINE_PARSE_OK;
}


int cmdline_parse(struct cmdline_parser *parser)
{
	int retval;
	cmdline_parse_phase_func func;
	parser->phase = 0;
	while (1) {
		func = phase_func[parser->phase];
		retval = (*func)(parser);
		if (retval != CMDLINE_PARSE_OK) {
			break; 
		}
	}
	return retval;
}

#define PUSHBACK_TOKEN(pool, toklist, tokentype, tokenflags, tokbegin, tokend) \
	ptok = p_alloc(pool, sizeof(struct token)); \
	ptok->type = tokentype; \
	ptok->flags = tokenflags; \
	ptok->tok.len = p - tokbegin; \
	ptok->tok.data = tokbegin; \
	l_pushback(toklist, ptok); \
	tokentype = 0; \
	tokenflags = 0; \
	tokbegin = p; \
	state = PARSE_STATE_BEGIN;


static int cmdline_parse_token(struct cmdline_parser *parser)
{
	int retval;
	struct mempool *pool;
	struct list *toklist;
	int state;
	const char *tokbegin;
	const char *p;
	const char *IFS;
	int tokentype;
	int tokenflags;
	struct token *ptok;
	struct cstr ttok;
	
	pool = parser->pool;
	toklist = parser->toklist;
	state = parser->state;
	tokbegin = parser->tokbegin;
	p = parser->p;
	IFS = parser->IFS;
	tokentype = 0;
	tokenflags = 0;
	retval = CMDLINE_PARSE_OK;
	while (*p != '\0') {
		switch (state) {
		case PARSE_STATE_BEGIN:
			tokbegin = get_token_begin(tokbegin, IFS);
			if (*tokbegin == '\0') {
				retval =  CMDLINE_PARSE_OK;
				goto RETURN;
			}
			p = tokbegin;
			switch(*p) {
			case '\'': state = PARSE_STATE_SQUOTE;++p;break;
			case '"':  state = PARSE_STATE_DQUOTE;++p;break;
			case '`':  state = PARSE_STATE_BACKTICK;++p;break;
			case '[':  state = PARSE_STATE_TEST;++p;break;
			case '|':
				tokentype = TOKEN_TYPE_PIPE;
				p = tokbegin+1;
				state = PARSE_STATE_DONE;
				break;
			case '&':
				if (p[1] == '&') {
					tokentype = TOKEN_TYPE_DAND;
					p = tokbegin +2;
				}
				else {
					tokentype = TOKEN_TYPE_AND;
					p = tokbegin +1;
				}
				state = PARSE_STATE_DONE;
				break;
			case ';':
				tokentype = TOKEN_TYPE_SIMICOLON;
				p = tokbegin +1;
				state = PARSE_STATE_DONE;
				break;
			case '<':
			case '>':
				state = PARSE_STATE_REDIRECT;
				p = tokbegin;
				break;
			case '(':
				parser->errmsg = "invalid syntax: unexpected '('";
				retval = CMDLINE_PARSE_SYNTAX_ERROR;
				goto RETURN;
			case ')':
				parser->errmsg = "invalid syntax: unexpected ')'";
				retval = CMDLINE_PARSE_SYNTAX_ERROR;
				goto RETURN;
			case '#':
				state = PARSE_STATE_COMMENT;
				break;
			case '\n':
				p = tokbegin +1;
				/*
				PUSHBACK_TOKEN(pool, toklist, TOKEN_TYPE_ENDLINE, 0, tokbegin, p);
				*/
				ptok = p_alloc(pool, sizeof(struct token)); 
				ptok->type = TOKEN_TYPE_ENDLINE;
				ptok->flags = tokenflags;
				ptok->tok.len = p - tokbegin;
				ptok->tok.data = tokbegin;
				l_pushback(toklist, ptok);
				tokentype = 0;
				tokenflags = 0;
				tokbegin = p;
				state = PARSE_STATE_BEGIN;
				break;
			default:
				p = tokbegin;
				state = PARSE_STATE_NORMAL;
				break;
			}
			break; /* goto next state */
		case PARSE_STATE_COMMENT:
			p = get_enclose(p, '\n');
			if (*p == '\0') {
				retval = CMDLINE_PARSE_CONTINUE;
				goto RETURN;
			}
			tokentype = TOKEN_TYPE_COMMENT;
			state = PARSE_STATE_DONE;
			break;
		case PARSE_STATE_SQUOTE:
			p = get_enclose(p, '\'');
			tokentype = TOKEN_TYPE_NORMAL;
			tokenflags |= TOKEN_FLAGS_SQUOTED;
			state = PARSE_STATE_DONE;
			break;
		case PARSE_STATE_DQUOTE:
			p = get_enclose(p, '"');
			tokentype = TOKEN_TYPE_NORMAL;
			tokenflags |= TOKEN_FLAGS_DQUOTED;
			state = PARSE_STATE_DONE;
			break;
		case PARSE_STATE_BACKTICK:
			p = get_enclose(p, '`');
			tokentype = TOKEN_TYPE_BACKTICK;
			state = PARSE_STATE_DONE;
			break;
		case PARSE_STATE_TEST:
			p = get_enclose(p, ']');
			tokentype = TOKEN_TYPE_TEST;
			state = PARSE_STATE_DONE;
			break;
		case PARSE_STATE_NORMAL:
			while (*p != '\0' && strchr(IFS, *p) == NULL) {
				if (*p == '\\') {
					++p;
					if (*p == '\0') {
						retval = CMDLINE_PARSE_CONTINUE;
						goto RETURN;
					}
					tokenflags |= TOKEN_FLAGS_BACKSLASH;
				}
				else if (*p == '(' && *(p+1) == ')') {
					ptok = p_alloc(pool, sizeof(struct token));
					ptok->type = TOKEN_TYPE_FUNCNAME;
					ptok->flags = 0;
					ptok->tok.len = p - tokbegin;
					ptok->tok.data = tokbegin;
					l_pushback(toklist, ptok);
					state = PARSE_STATE_FUNCBODY;
					tokentype = 0;
					tokenflags = 0;
					tokbegin = p +2;
					break;
				}
				else if (*p == '>' || *p == '<') {
					ttok.data = tokbegin;
					ttok.len = p - tokbegin;
					if (cstr_isnumeric(&ttok)) {
						state = PARSE_STATE_REDIRECT;
					}
					else {
						tokentype = TOKEN_TYPE_NORMAL;
						state = PARSE_STATE_DONE;
					}
					break;
				}
				++p;
			}
			if (state != PARSE_STATE_NORMAL) {
				break;
			}
			tokentype = TOKEN_TYPE_NORMAL;
			state = PARSE_STATE_DONE;
			break;
		case PARSE_STATE_FUNCBODY:
			tokbegin = get_token_begin(tokbegin, IFS);
			if (*tokbegin == '\0') {
				retval = CMDLINE_PARSE_CONTINUE;
				goto RETURN;
			}
			if (*tokbegin != '{' ) {
				parser->errmsg = "invalid syntax: expected '{'";
				retval = CMDLINE_PARSE_SYNTAX_ERROR;
				goto RETURN;
			}
			p = get_function_end(tokbegin);
			if (*p == '\0') {
				retval = CMDLINE_PARSE_CONTINUE;
				goto RETURN;
			}
			tokentype = TOKEN_TYPE_FUNCBODY;
			state = PARSE_STATE_DONE;
			break;
		case PARSE_STATE_REDIRECT:
			if (*p == '>' && *(p+1) == '>') {
				p+=1;
			}
			if (*(p+1) == '&') {
				p = get_token_end(p+2, IFS);
			}
			else {
				/*
				p = get_token_begin(p+1, IFS);
				if (*p == '\n') {
					parser->errmsg = "unexpected newline";
					retval = CMDLINE_PARSE_SYNTAX_ERROR;
					goto RETURN;
				}
				p = get_token_end(p, IFS);
				*/
				p += 1;
			}
			if (*p == '\0') {
				retval = CMDLINE_PARSE_CONTINUE;
				goto RETURN;
			}
			tokentype = TOKEN_TYPE_REDIRECT;
			state = PARSE_STATE_DONE;
			break;
		case PARSE_STATE_DONE:
			ptok = p_alloc(pool, sizeof(struct token));
			ptok->type = tokentype;
			ptok->flags = tokenflags;
			ptok->tok.len = p - tokbegin;
			ptok->tok.data = tokbegin;
			l_pushback(toklist, ptok);
			tokentype = 0;
			tokenflags = 0;
			tokbegin = p;
			state = PARSE_STATE_BEGIN;
			break;
		}
	}
RETURN:
	parser->state = state;
	parser->tokbegin = tokbegin;
	parser->p = p;
	parser->phase += 1;
	return retval;
}


static int cmdline_parse_classication(struct cmdline_parser *parser)
{
	struct mempool *pool;
	struct lnode *node;
	struct token *token;
	struct list *cmdlist;
	struct list *infos;
	struct process_startup_info *info;
	struct redirection *re;
	int retval;
	int redirect_file;
	
	pool = parser->pool;
	cmdlist = parser->cmdlist;
	if (cmdlist == NULL) {
		cmdlist = l_create(pool);
	}
	infos = l_create(pool);
	info = create_startup_info(pool);
	redirect_file =0;
	for (node = parser->toklist->first; node != NULL; node = node->next) {
		token = node->data;
		if (redirect_file) {
			switch(token->type) {
			case TOKEN_TYPE_NORMAL:
				if (token->flags & (TOKEN_FLAGS_DQUOTED | TOKEN_FLAGS_SQUOTED)) {
					re->right.pathname =  make_cstr(pool, token->tok.data+1, 
							token->tok.len-2);
				}
				else {
					re->right.pathname = &(token->tok);
				}
			default:
				parser->errmsg = "invalid syntax";
				return CMDLINE_PARSE_SYNTAX_ERROR;
			}
			redirect_file = 0;
			continue;
		}
		switch (token->type) {
		case TOKEN_TYPE_NORMAL:
			if (token->flags & (TOKEN_FLAGS_DQUOTED | TOKEN_FLAGS_SQUOTED)) {
				l_pushback(info->params, make_cstr(pool, token->tok.data+1,
							token->tok.len-2));
			}
			else {
				l_pushback(info->params, &(token->tok));
			}
			break;
		case TOKEN_TYPE_REDIRECT:
			re = p_alloc(pool, sizeof(struct redirection));
			retval = cmdline_parse_redirection(parser, &(token->tok), re);
			if (retval != CMDLINE_PARSE_OK) {
				return retval;
			}
			if (re->flags & REDIRECT_FILE) {
				redirect_file = 1;
			}
			l_pushback(info->redirections, re);
			break;
		case TOKEN_TYPE_ENDLINE:
		case TOKEN_TYPE_SIMICOLON:
			if (!startupinfo_empty(info)) {
				l_pushback(infos, info);
				info = create_startup_info(pool);
			}
			if (!l_empty(infos)) {
				l_pushback(cmdlist, infos);
				infos = l_create(pool);
			}
			break;
		case TOKEN_TYPE_PIPE:
			if (startupinfo_empty(info)) {
				parser->errmsg = "invalid syntax";
				return CMDLINE_PARSE_SYNTAX_ERROR;
			}
			else {
				l_pushback(infos, info);
				info = create_startup_info(pool);
			}
			break;
		case TOKEN_TYPE_COMMENT:
			break;
		default:
			l_pushback(infos, info);
			info = create_startup_info(pool);
			break;
		}
	}
	parser->phase += 1;
	return CMDLINE_PARSE_DONE;
}


#ifdef DEBUG
static int cmdline_parse_expand(struct cmdline_parser *parser)
{
	return PARSE_STATE_DONE;
}


static int cmdline_parse_token_print(struct cmdline_parser *parser)
{
	int i, count;
	struct lnode *node;
	struct token *tok;
	count = l_count(parser->toklist);
	fprintf(stderr, "got %d tokens:\n", count);
	for (i=0,node = parser->toklist->first; i< count; ++i,node = node->next) {
		tok = node->data;
		printf(" %d %d %d: ", i, tok->type, tok->flags);
		cstr_print(&tok->tok, stdout);
		printf("\n");
	}
	parser->phase += 1;
	return CMDLINE_PARSE_OK;
}


static int cmdline_parse_end(struct cmdline_parser *parser)
{
	parser->errmsg = "parse end;";
	return CMDLINE_PARSE_SYNTAX_ERROR;
}
#endif


const char *errmsg(struct cmdline_parser *parser)
{
	return parser->errmsg;
}


