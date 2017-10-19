#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "parser.h"
#include "mempool.h"
#include "list.h"
#include "str.h"
#include "env.h"

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

#define PARSE_STATE_SIMPLE 		0x01
#define PARSE_STATE_BEGIN		0x02
#define PARSE_STATE_NORMAL 		0x03
/*
#define PARSE_STATE_DONE		0x04
*/
#define PARSE_STATE_SQUOTE 		0x05
#define PARSE_STATE_DQUOTE 		0x06
#define PARSE_STATE_BACKTICK 	0x08
/*
#define PARSE_STATE_ENCLOSE		0x06
*/
#define PARSE_STATE_BACKSLASH 	0x09
#define PARSE_STATE_REDIRECT	0x10
#define PARSE_STATE_ASSIGNMENT	0x11
#define PARSE_STATE_COMMENT		0x13
#define PARSE_STATE_JOB			0x14

#define PARSE_PHASE_TOKEN	0x00
#define PARSE_PHASE_ESCAPE  0x01
#define PARSE_PHASE_COUNT   0x02


struct parser
{
	struct mempool *pool;
	struct list *cmdlist;
	struct cmdline_buf *cmdlinebuf;
	struct env *env;

	int phase;
	int state;
	const char *p;
	struct simple_command *cmd;
	struct list *tokenlist;
	const char *errmsg;
};

typedef int (*parser_parse_phase_func)(struct parser *);
static int parser_parse_rawtoken(struct parser *parser);
static int parser_ensure_line_ending(struct parser *parser);
static int parser_parse_classication(struct parser *parser);
#ifdef DEBUG
static int parser_parse_token_print(struct parser *parser);
#endif

static parser_parse_phase_func phase_func[] = {
	&parser_parse_rawtoken,
#ifdef DEBUG
	&parser_parse_token_print,
#endif
	&parser_ensure_line_ending,
	&parser_parse_classication,
};


#define TOKEN_TYPE_SIMPLE                0x00
#define TOKEN_TYPE_NORMAL                0x01
#define TOKEN_TYPE_REDIRECT              0x02
#define TOKEN_TYPE_PIPE                  0x03
#define TOKEN_TYPE_AND                   0x04
#define TOKEN_TYPE_LOGIC_AND             0x05
#define TOKEN_TYPE_LOGIC_OR              0x06
#define TOKEN_TYPE_SEMICOLON             0x07
#define TOKEN_TYPE_LINE_ENDING           0x08
#define TOKEN_TYPE_TERMINATE             0x09
#define TOKEN_TYPE_COMMENT               0x0a
#define TOKEN_TYPE_TEST                  0x0b
#define TOKEN_TYPE_JOB                   0x0c
#define TOKEN_TYPE_FUNCNAME              0x0d
#define TOKEN_TYPE_FUNCBODY              0x0f
#define TOKEN_TYPE_BACKTICK              0x10
#define TOKEN_TYPE_VARIABLE              0x11
#define TOKEN_TYPE_SPECIALVAR            0x12
#define TOKEN_TYPE_SUBCMDLINE            0x13
#define TOKEN_TYPE_DQUOTED               0x14
#define TOKEN_TYPE_SQUOTED               0x15
#define TOKEN_TYPE_BACKSLASH             0x16

#define TOKEN_FLAGS_VARIABLE             0x01
#define TOKEN_FLAGS_SPECIALVAR           0x02
#define TOKEN_FLAGS_WILDCARD             0x04
#define TOKEN_FLAGS_INLINE_SUBPROCESS    0x08
#define TOKEN_FLAGS_DQUOTED              0x0F
#define TOKEN_FLAGS_SQUOTED              0x10
#define TOKEN_FLAGS_BACKSLASH            0x20
#define TOKEN_FLAGS_ASSIGNMENT           0x40

struct token {
	unsigned int type:16;
	unsigned int flags:16;
	struct str value;
};


struct parser *create_parser(struct mempool *pool, 
		struct list *cmdlist, struct cmdline_buf *buf,
		struct env *env)
{
	struct parser *parser;
	parser = p_alloc(pool, sizeof(struct parser));
	parser->pool = pool;
	parser->cmdlist = cmdlist;
	parser->cmdlinebuf = buf;
	parser->env = env;
	parser->tokenlist = l_create(pool);
	parser->phase = PARSE_PHASE_TOKEN;
	parser->state = PARSE_STATE_BEGIN;
	parser->p = buf->data;
	return parser;
}

static const char *get_enclose(const char *p, int ch)
{
	for (;*p != '\0';p++) {
		if (*p == ch) {
			return p;
		}
		if (*p == '\\') {
			p++;
			if (*p == '\0') {
				break;
			}
		}
	}
	return p;
}


const char *get_token_start(const char *p, const char *IFS)
{
	for (; p && *p != '\0' && *p != '\n' && strchr(IFS, *p) != NULL; ++p);
	return p;
}


const char *get_token_finish(const char *p, const char *IFS)
{
	for (; p && *p != '\0' && strchr(IFS, *p) == NULL; ++p);
	return p;
}


int parser_parse_redirection(struct parser *parser, struct str *token, 
		struct redirection *re)
{
	const char *p;
	struct str t;
	int leftdeffd;
	
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
		re->leftfd = s_atoi(&t);
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
		re->right.fd = s_atoi(&t);
	}
	else {
		re->flags |= REDIRECT_OUT;
	}
	return CMDLINE_PARSE_OK;
}


int parser_parse(struct parser *parser)
{
	int retval;
	parser_parse_phase_func func;
	parser->phase = 0;
	while (1) {
		func = phase_func[parser->phase];
		retval = (*func)(parser);
		if (retval != CMDLINE_PARSE_OK) {
			break; 
		}
		parser->phase += 1;
	}
	return retval;
}


static int parser_parse_rawtoken(struct parser *parser)
{
	int retval;
	struct mempool *pool;
	struct list *tokenlist;
	const char *p;
	const char *start;
	const char *finish;
	int state;
	int flags;
	struct token *token;
	struct str tmp;
	
	pool = parser->pool;
	tokenlist = parser->tokenlist;
	state = parser->state;
	start = parser->cmdlinebuf->data;
	p = parser->p;
	flags = 0;
	retval = CMDLINE_PARSE_OK;

#define PUSHBACK_TOKEN(ENDOFFSET, TOKENTYPE, TOKENFLAGS) \
	p += ENDOFFSET; \
	token = p_alloc(pool, sizeof(struct token)); \
	token->type = TOKENTYPE; \
	token->flags = flags | TOKENFLAGS;\
	token->value.len = p - start; \
	token->value.data = p_strndup(pool, start, p-start); \
	l_pushback(tokenlist, token); \
	flags = 0; \
	start = p; \
	state = PARSE_STATE_BEGIN;

#define ERROR_RETURN(MSG, RETCODE) \
	parser->errmsg = MSG; \
	retval = RETCODE; \
	goto RETURN; 

#define ENSURE_NOT_END(CH, RETVAL) \
	if (CH == '\0') { \
		retval = RETVAL; \
		goto RETURN; \
	}

	while (*p != '\0') {
		switch (state) {
		case PARSE_STATE_BEGIN:
			p = start = get_token_start(start, parser->env->IFS);
			if (*p == '\0') {
				retval =  CMDLINE_PARSE_OK;
				goto RETURN;
			}
			switch(*p) {
			case '\'': state = PARSE_STATE_SQUOTE;++p;break;
			case '"':  state = PARSE_STATE_DQUOTE;++p;break;
			case '`':  state = PARSE_STATE_BACKTICK;++p;break;
			case '|':  
				ENSURE_NOT_END(p[1], CMDLINE_PARSE_CONTINUE);
				if (p[1] == '|') {
					PUSHBACK_TOKEN(2, TOKEN_TYPE_LOGIC_OR, 0);
				}
				else {
					PUSHBACK_TOKEN(1, TOKEN_TYPE_PIPE, 0);
				}
				break;
			case '&':
				ENSURE_NOT_END(p[1], CMDLINE_PARSE_CONTINUE);
				if (p[1] == '&') {
					PUSHBACK_TOKEN(2, TOKEN_TYPE_LOGIC_AND, 0);
				}
				else {
					PUSHBACK_TOKEN(1, TOKEN_TYPE_AND, 0);
				}
				break;
			case ';':PUSHBACK_TOKEN(1, TOKEN_TYPE_SEMICOLON, 0);break;
			case '<':
			case '>':
				 state = PARSE_STATE_REDIRECT;
				 break;
			case '(': ERROR_RETURN("invalid syntax: unexpected '('", CMDLINE_PARSE_SYNTAX_ERROR);
			case ')': ERROR_RETURN("invalid syntax: unexpected ')'", CMDLINE_PARSE_SYNTAX_ERROR);
			case '#': state = PARSE_STATE_COMMENT; break;
			case '\n': PUSHBACK_TOKEN(1, TOKEN_TYPE_LINE_ENDING, 0);break;
			default: state = PARSE_STATE_NORMAL; break;
			}
			break; /* goto next state */
		case PARSE_STATE_COMMENT:
			p = get_enclose(p, '\n');
			ENSURE_NOT_END(*p, CMDLINE_PARSE_CONTINUE);
			PUSHBACK_TOKEN(1, TOKEN_TYPE_COMMENT, 0);
			break;
		case PARSE_STATE_SQUOTE:
			p = get_enclose(p, '\'');
			ENSURE_NOT_END(*p, CMDLINE_PARSE_CONTINUE);
			PUSHBACK_TOKEN(1, TOKEN_TYPE_NORMAL, TOKEN_FLAGS_SQUOTED);
			break;
		case PARSE_STATE_DQUOTE:
			p = get_enclose(p, '"');
			ENSURE_NOT_END(*p, CMDLINE_PARSE_CONTINUE);
			PUSHBACK_TOKEN(1, TOKEN_TYPE_NORMAL, TOKEN_FLAGS_DQUOTED);
			break;
		case PARSE_STATE_BACKTICK:
			p = get_enclose(p, '`');
			ENSURE_NOT_END(*p, CMDLINE_PARSE_CONTINUE);
			PUSHBACK_TOKEN(1, TOKEN_TYPE_NORMAL, 0);
			break;
		case PARSE_STATE_NORMAL:
			switch (*p) {
			case '\\':
				ENSURE_NOT_END(*(p+1), CMDLINE_PARSE_CONTINUE);
				flags |= TOKEN_FLAGS_BACKSLASH;
				p+=2;
				break;
			case '(' :
				ERROR_RETURN("function not support yet",
						CMDLINE_PARSE_SYNTAX_ERROR);
				break;
			case '=':
				flags |= TOKEN_FLAGS_ASSIGNMENT;
				p++;
				break;
			case '>':
			case '<':
				tmp.data = start;
				tmp.len = p - start;
				if (p != start && !s_isnumeric(&tmp)) {
					PUSHBACK_TOKEN(0, TOKEN_TYPE_NORMAL, 0);
				}
				state = PARSE_STATE_REDIRECT;
				break;
			default:
				if (strchr(parser->env->IFS, *p) != NULL) {
					PUSHBACK_TOKEN(0, TOKEN_TYPE_NORMAL, 0);
					break;
				}
				p++;
				break;
			}
			break;
		case PARSE_STATE_REDIRECT:
			if (*p == '>' && *(p+1) == '>') {
				p+=1;
			}
			if (*(p+1) == '&') {
				ENSURE_NOT_END(*(p+2), CMDLINE_PARSE_CONTINUE);
				finish = get_token_finish(p+2, parser->env->IFS);
				ENSURE_NOT_END(*finish, CMDLINE_PARSE_CONTINUE);
				p = finish;
			}
			else {
				p += 1;
			}
			PUSHBACK_TOKEN(0, TOKEN_TYPE_REDIRECT, 0);
			break;
		default :
			ERROR_RETURN("parse error", CMDLINE_PARSE_SYNTAX_ERROR);
			break;
		}
	}
RETURN:
	cmdline_buf_parsed(parser->cmdlinebuf, start);
	parser->p = p - (start - parser->cmdlinebuf->data);
	parser->state = state;
	return retval;
}


static int parser_ensure_line_ending(struct parser *parser)
{
	struct lnode *node;
	struct token *token;

	node = parser->tokenlist->last;
	if (node == NULL) {
		return CMDLINE_PARSE_CONTINUE;
	}
	token = node->data;
	if (token->type == TOKEN_TYPE_LINE_ENDING) {
		return CMDLINE_PARSE_OK;
	}
	return CMDLINE_PARSE_CONTINUE;
}


static int parser_parse_classication(struct parser *parser)
{
	struct mempool      *pool;
	struct lnode        *node;
	struct token        *token;
	struct list         *cmdlist;
	struct complex_command *pipecmd;
	struct complex_command *logiccmd;
	struct simple_command      *cmd;
	void *c;
	struct redirection  *re;
	int                  retval;
	BOOL                 redirect_file;
	BOOL                 parsingenv;
	
	pool = parser->pool;
	cmdlist = parser->cmdlist;
	if (cmdlist == NULL) {
		cmdlist = l_create(pool);
	}
	pipecmd = NULL;
	logiccmd = NULL;
	cmd = create_command(pool);
	re = NULL;
	redirect_file = FALSE;
	parsingenv = TRUE;


	for (node = parser->tokenlist->first; node != NULL; node = node->next) {
		token = node->data;
		if (redirect_file) {
			switch(token->type) {
			case TOKEN_TYPE_NORMAL:
				if (token->flags & (TOKEN_FLAGS_DQUOTED | TOKEN_FLAGS_SQUOTED)) {
					re->right.pathname =  s_make(pool, token->value.data+1, 
							token->value.len-2);
				}
				else {
					re->right.pathname = &(token->value);
				}
				break;
			default:
				parser->errmsg = "invalid syntax";
				return CMDLINE_PARSE_SYNTAX_ERROR;
			}
			redirect_file = FALSE;
			continue;
		}
		switch (token->type) {
		case TOKEN_TYPE_NORMAL:
			if (parsingenv) {
				if (token->flags & TOKEN_FLAGS_ASSIGNMENT && !(token->flags &
						(TOKEN_FLAGS_DQUOTED | TOKEN_FLAGS_SQUOTED))) {
					l_pushback(cmd->envp, &(token->value));
					break;
				}
				else {
					parsingenv = FALSE;
				}
			}
			if (token->flags & (TOKEN_FLAGS_DQUOTED | TOKEN_FLAGS_SQUOTED)) {
				l_pushback(cmd->params, s_make(pool, token->value.data+1,
							token->value.len-2));
			}
			else {
				l_pushback(cmd->params, &(token->value));
			}
			break;
		case TOKEN_TYPE_REDIRECT:
			re = p_alloc(pool, sizeof(struct redirection));
			retval = parser_parse_redirection(parser, &(token->value), re);
			if (retval != CMDLINE_PARSE_OK) {
				return retval;
			}
			if (re->flags & REDIRECT_OUT) {
				redirect_file = TRUE;
			}
			l_pushback(cmd->redirections, re);
			break;
		case TOKEN_TYPE_LINE_ENDING:
		case TOKEN_TYPE_SEMICOLON:
			if (!command_empty(cmd)) {
				cmd->sep = CMD_SEPERATOR_END;
				c = cmd;
				if (pipecmd != NULL) {
					l_pushback(pipecmd->commands, c);
					c = pipecmd;
					pipecmd = NULL;
				}
				if (logiccmd != NULL) {
					l_pushback(logiccmd->commands, c);
					c = logiccmd;
					logiccmd = NULL;
				}
				l_pushback(cmdlist, c);
				cmd = create_command(pool);
			}
			break;
		case TOKEN_TYPE_PIPE:
			if (command_empty(cmd)) {
				parser->errmsg = "invalid syntax";
				return CMDLINE_PARSE_SYNTAX_ERROR;
			}
			if (pipecmd == NULL) {
				pipecmd = create_pipe_command(pool);
			}
			cmd->sep = CMD_SEPERATOR_PIPE;
			l_pushback(pipecmd->commands, cmd);
			cmd = create_command(pool);
			break;
		case TOKEN_TYPE_COMMENT:
			break;
		case TOKEN_TYPE_LOGIC_AND:
			cmd->sep = CMD_SEPERATOR_LOGIC_AND;
			if (logiccmd == NULL) {
				logiccmd = create_logic_command(pool);
			}
			if (pipecmd != NULL) {
				l_pushback(pipecmd->commands, cmd);
				l_pushback(logiccmd->commands, pipecmd);
				pipecmd = NULL;
			}
			else {
				l_pushback(logiccmd->commands, cmd);
			}
			cmd = create_command(pool);
			break;
		case TOKEN_TYPE_LOGIC_OR:
			cmd->sep = CMD_SEPERATOR_LOGIC_OR;
			if (logiccmd == NULL) {
				logiccmd = create_logic_command(pool);
			}
			if (pipecmd != NULL) {
				l_pushback(pipecmd->commands, cmd);
				l_pushback(logiccmd->commands, pipecmd);
				pipecmd = NULL;
			}
			else {
				l_pushback(logiccmd->commands, cmd);
			}
			cmd = create_command(pool);
			break;
		case TOKEN_TYPE_AND:
			/*TODO*/
		default:
			parser->errmsg = "unexpected token type";
			return CMDLINE_PARSE_SYNTAX_ERROR;
			break;
		}
	}
	return CMDLINE_PARSE_DONE;
}


#ifdef DEBUG
static int parser_parse_token_print(struct parser *parser)
{
	int i, count;
	struct lnode *node;
	struct token *token;
	count = l_count(parser->tokenlist);
	printf("got %d tokens:\n", count);
	for (i=0,node = parser->tokenlist->first; i< count; ++i,node = node->next) {
		token = node->data;
		printf(" %d %d %d: ", i, token->type, token->flags);
		if (token->type == TOKEN_TYPE_LINE_ENDING) {
			printf("\\n");
		}
		else {
			s_print(&token->value, stdout);
		}
		printf("\n");
	}
	return CMDLINE_PARSE_OK;
}

#endif


const char *errmsg(struct parser *parser)
{
	return parser->errmsg;
}


