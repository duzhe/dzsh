#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include "mempool.h"
#include "list.h"
#include "str.h"
#include "parser.h"
#include "env.h"


const char *getfullpathname(struct mempool *pool, struct str *name)
{
	struct str *cwd;
	char *pathname;
	int len;
	if (*(name->data) == '/') {
		return p_sdup(pool, name);
	}
	cwd = env->cwd;
	len = cwd->len + 1 + name->len;
	pathname = p_alloc(pool, len+1);
	memcpy(pathname, cwd->data, cwd->len);
	pathname[cwd->len] = '/';
	memcpy(pathname+ cwd->len +1, name->data, name->len);
	return pathname;
}


int do_redirect(struct mempool *pool, struct list *redirections)
{
	struct redirection *p;
	int retval;
	const char *filename;
	struct lnode *node;
	int rightfd;
	const char *openfor;
	for (node = redirections->first; node != NULL; node = node->next) {
		p = node->data;
		if (p->flags & REDIRECT_OUT) {
			filename = getfullpathname(pool, p->right.pathname);
			if (p->flags & REDIRECT_IN) {
				rightfd = open(filename,  O_RDONLY);
				openfor = "read";
			}
			else {
				if (p->flags & REDIRECT_APPEND) {
					rightfd = open(filename, O_CREAT|O_APPEND|O_WRONLY,
							S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
				}
				else {
					rightfd = open(filename, O_CREAT|O_TRUNC|O_WRONLY,
							S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
				}
				openfor = "write";
			}
			if (rightfd == -1) {
				fprintf(stderr, "%s: cannot open file ", env->argv[0]);
				s_print(p->right.pathname, stderr);
				fprintf(stderr, " for %s: %s\n", openfor, strerror(errno));
				return -1;
			}
		}
		else {
			rightfd = p->right.fd;
		}

		retval = dup2(rightfd, p->leftfd);
		if (p->flags & REDIRECT_OUT) {
			close(rightfd);
		}

		if (retval == -1) {
			fprintf(stderr, "%s: cannot redirect file: %s\n", env->argv[0], 
					strerror(errno));
			return -1;
		}
	}
	return 0;
}


static int redirect_and_exec(struct mempool *pool, const char *bin,
		char **params, char **envp, struct list *redirections, int fdin,
		int fdout)
{
	int retval;
	if (fdin != STDIN_FILENO) {
		/*
		fprintf(stderr, "pid: %d, dup2: %d ->  %d \n", getpid(), fdin, STDIN_FILENO);
		*/
		dup2(fdin, STDIN_FILENO);
		close(fdin);
	}
	if (fdout != STDOUT_FILENO) {
		/*
		fprintf(stderr, "pid: %d, dup2: %d ->  %d \n", getpid(), fdout, STDOUT_FILENO);
		*/
		dup2(fdout, STDOUT_FILENO);
		close(fdout);
	}
	if (do_redirect(pool, redirections) == -1) {
		return -1;
	}
	if (bin != NULL) {
		retval = execve(bin, params, envp);
		if (retval == -1) {
			fprintf(stderr, "errno: %d\n", errno);
			switch(errno) {
			case EACCES:
				fprintf(stderr, "%s: %s: Permission denied\n", env->argv[0],
						params[0]);
				return 126;
			default:
				fprintf(stderr, "%s: %s Other Error\n", env->argv[0],
						params[0]);
				return 127;
			}
		}
	}
	return 0; 
}


static const char *getbinpathname(struct mempool *pool, struct str *bin,
		struct list *pathentry)
{
	struct lnode *node;
	size_t len;
	char *pathname;
	struct str *binpath;
	struct stat statbuf;
	int retval;
	if (s_empty(bin)) {
		return NULL;
	}
	switch(*(bin->data)) {
	case '/':
		pathname = (char*)bin->data;
		len = bin->len;
		break;
	case '.':
		len = env->cwd->len + 1 + bin->len;
		pathname = p_alloc(pool, len +1);
		memcpy(pathname, env->cwd->data, env->cwd->len);
		pathname[env->cwd->len] = '/';
		memcpy(pathname + env->cwd->len +1, bin->data, bin->len);
		pathname[len] = '\0';
		break;
	default:
		pathname = p_large_alloc(pool, env->maxpath);
		for (node=pathentry->first; node!=NULL; node=node->next) {
			binpath = node->data;
			len = binpath->len + 1 + bin->len;
			memcpy(pathname, binpath->data, binpath->len);
			pathname[binpath->len] = '/';
			memcpy(pathname + binpath->len +1, bin->data, bin->len);
			pathname[len] = '\0';
			retval = stat(pathname, &statbuf);
			if (retval == 0 && statbuf.st_mode & S_IFREG) {
				return pathname;
			}
			*pathname = '0';
		}
		break;
	}
	if (*pathname != '/') {
		return NULL;
	}
	/* test exists */
	retval = stat(pathname, &statbuf);
	if (retval != 0 || !(statbuf.st_mode & S_IFREG)) {
		return NULL;
	}
	return pathname;
}


static int resolve(struct mempool *pool, struct simple_command *cmd)
{
	struct str *bin;
	const char *binfullpath;
	cmd->bin = NULL;
	if (cmd->params->first != NULL) {
		bin = cmd->params->first->data;
		binfullpath = getbinpathname(pool, bin, env->pathentry);
		if (binfullpath == NULL) {
			fprintf(stderr, "%s: %s: command not found\n", env->argv[0],
					bin->data);
			return 127;
		}
		cmd->bin = binfullpath;
	}
	return 0;
}


static int execute_command(struct mempool *pool, struct command *cmd);


static int execute_simple(struct mempool *pool, struct simple_command *cmd,
		struct list *sonpids, int fdin, int fdout)
{
	int paramscount;
	struct lnode *paramsnode;
	char **params;
	char **envp;
	int i;
	pid_t pid;
	pid_t *pidpointer;
	int retval;
	int exitstatus;

	/* get an array form 'params' */
	paramscount = l_count(cmd->params);
	params = p_alloc(pool, sizeof(char*) *(paramscount+1));
	i = 0;
	for (paramsnode = cmd->params->first; paramsnode != NULL;
			paramsnode = paramsnode->next) {
		params[i++] = p_sdup(pool, paramsnode->data);
	}
	params[i] = NULL;

	/* gen an array form envp if envp is not empty */
	paramscount = l_count(cmd->envp);
	if (paramscount != 0) {
		envp = p_alloc(pool, sizeof(char*) *(paramscount+1));
		i = 0;
		for (paramsnode = cmd->envp->first; paramsnode != NULL;
				paramsnode = paramsnode->next) {
			envp[i++] = p_sdup(pool, paramsnode->data);
		}
		envp[i] = NULL;
	}
	else {
		envp = NULL;
	}

	/* fork and execute */
	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s\n", strerror(errno));
		return 255;
	}
	if (pid == 0) {
		/* create a new mempool to avoid COW */
		pool = p_create(8196);
		retval = resolve(pool, cmd);
		if (retval != 0) {
			p_destroy(pool);
			exit(retval);
		}
		/* on success, redirect and exec will not return */
		retval = redirect_and_exec(pool, cmd->bin, params, envp,
				cmd->redirections, fdin, fdout);
		p_destroy(pool);
		exit(retval);
	}
	/* save son process pid */
	if (sonpids != NULL) {
		pidpointer = p_alloc(pool, sizeof(pid_t));
		*pidpointer = pid;
		l_pushback(sonpids, pidpointer);
		return 0;
	}
	waitpid(pid, &exitstatus, 0);
	return exitstatus;
}


static int execute_pipe(struct mempool *pool, struct complex_command *cmd)
{
	int fdin, fdout;
	struct lnode *node;
	struct simple_command *curr;
	struct list *sonpids;
	int pipefd[2];
	int retval;
	pid_t *pidpointer;
	int exitstatus;

	fdin = STDIN_FILENO;
	fdout = STDOUT_FILENO;
	sonpids = l_create(pool);

	for (node = cmd->commands->first; node != NULL;
			node = node->next) {
		/* prepare pipes */
		/* pipefd[1] is for write , using it as STDOUT at the previous 
		 * process (on the left side of '|' sign), pipefd[0] is for read, 
		 * using it as STDIN at the next process (on the right side of '|'
		 * sign)
		 * first process, using STDIN as STDIN.
		 * the last process , using STDOUT as STDOUT.
		 */
		curr = node->data;
		if (node->next != NULL) {
			retval = pipe(pipefd);
			if (retval == -1) {
				fprintf(stderr, "%s: fail create pipe: %s\n", env->argv[0], 
						strerror(errno));
				return 255;
			}
			fdout = pipefd[1];
		}
		else {
			fdout = STDOUT_FILENO;
		}
		/* now we asume that there are only simple command in pipe command */
		execute_simple(pool, curr, sonpids, fdin, fdout);
		/* if 'fdin' or 'fdout' is pipe, close it */
		if (fdin != STDIN_FILENO) {
			close(fdin);
		}
		if (fdout != STDOUT_FILENO) {
			close(fdout);
		}
		fdin = pipefd[0];
	}
	/* after the last fork, wait for all son process */
	for (node = sonpids->first; node != NULL; 
			node = node->next) {
		pidpointer = node->data;
		waitpid(*pidpointer, &exitstatus, 0);
	}
	return exitstatus;
}


static int execute_logic(struct mempool *pool, struct complex_command *cmd)
{
	struct lnode *cmdnode;
	struct command *curr;
	int exitstatus;
	cmdseperator sep;

	sep = CMD_SEPERATOR_NONE;
	exitstatus = 0;
	for (cmdnode = cmd->commands->first; cmdnode != NULL;
			cmdnode = cmdnode->next) {
		curr = cmdnode->data;

		if (sep == CMD_SEPERATOR_LOGIC_AND && exitstatus != 0) {
			sep = command_get_seperator(curr);
			continue;
		}
		if (sep == CMD_SEPERATOR_LOGIC_OR && exitstatus == 0) {
			sep = command_get_seperator(curr);
			continue;
		}
		exitstatus = execute_command(pool, curr);
		sep = command_get_seperator(curr);
	}
	return exitstatus;
}

static int execute_command(struct mempool *pool, struct command *cmd)
{
	switch(cmd->type) {
		case CMD_TYPE_SIMPLE:
			return  execute_simple(pool, (struct simple_command *)cmd, NULL,
					STDIN_FILENO, STDOUT_FILENO);
		case CMD_TYPE_PIPE:
			return execute_pipe(pool, (struct complex_command *)cmd);
		case CMD_TYPE_LOGIC:
			return execute_logic(pool, (struct complex_command *)cmd);
		default:
			/* assert(0)*/
			break;
	}
	/* assert(0)*/
	return 0;
}


#ifdef PRINT_COMMAND_ONLY
static void print_command(struct command *cmd)
{
	struct simple_command *c;
	struct lnode *node;
	struct str *param;
	struct redirection *re;
	cmdseperator sep;

	if (cmd->type == CMD_TYPE_SIMPLE) {
		c = (struct simple_command *)cmd;
		for (node = c->envp->first; node != NULL; node = node->next) {
			param = node->data;
			s_print(param, stdout);
			fputc(' ', stdout);
		}
		for (node = c->params->first; node != NULL; node = node->next) {
			param = node->data;
			s_print(param, stdout);
			fputc(' ', stdout);
		}
		for (node = c->redirections->first; node != NULL; node = node->next) {
			re = node->data;
			switch (re->flags) {
				case 0:
					printf("fd%d->fd%d ", re->leftfd, re->right.fd);
					break;
				case 1:
					printf("fd%d<-fd%d ", re->right.fd, re->leftfd);
					break;
				case 2:
					printf("fd%d->", re->leftfd);
					s_print(re->right.pathname, stdout);
					fputc(' ', stdout);
					break;
				case 3:
					printf("fd%d<-", re->leftfd);
					s_print(re->right.pathname, stdout);
					fputc(' ', stdout);
					break;
				case 6:
					printf("fd%d->>", re->leftfd);
					s_print(re->right.pathname, stdout);
					fputc(' ', stdout);
					break;
				default:
					printf("unexpected flags %d\n", re->flags);
					break;
			}
		}
	}
	else if (cmd->type == CMD_TYPE_PIPE) {
		for (node = ((struct complex_command *)cmd)->commands->first;
				node != NULL; node = node->next) {
			print_command(node->data);
			if (node->next != NULL) {
				printf(" | ");
			}
		}
	}
	else if (cmd->type == CMD_TYPE_LOGIC) {
		for (node = ((struct complex_command *)cmd)->commands->first;
				node != NULL; node = node->next) {
			print_command(node->data);

			if (node->next != NULL) {
				sep = command_get_seperator(node->data);
				printf(sep == CMD_SEPERATOR_LOGIC_AND ? " && " : " || ");
			}
		}
	}
}
#endif


BOOL is_console(int fd)
{
	int retval;
	struct stat st;
	memset(&st, 0, sizeof(st));
	retval = fstat(fd, &st);
	if (retval != 0) {
		return FALSE;
	}
	return S_ISCHR(st.st_mode);
}


int main(int argc, char **argv)
{
	struct mempool *static_pool;
	struct cmdline_buf buf;
	const char *line;
	int retval;
	FILE *instream;
	struct list *cmdlist;
	struct command *cmd;
	struct mempool *pool;
	struct parser *parser;
	struct lnode *node;
	BOOL interactive;
	BOOL line_complete;

	/* global initialize */
	static_pool = p_create(8196);
	if (initialize_env(static_pool, argc, argv) != 0) {
		return -1;
	}

	/* parse dzsh commandline */
	if (argc == 1) {
		instream = stdin;
	}
	else {
		if (argc > 2) {
			fprintf(stderr, "Warning: accept one argument only," \
					"more arguments will be ignored\n");
		}
		instream = fopen(argv[1], "r");
		if (instream == NULL) {
			fprintf(stderr, "%s: cannot access file %s: %s\n", argv[0], 
					argv[1], strerror(errno));
			return 1;
		}
	}

	if (instream == stdin && is_console(STDIN_FILENO)) {
		interactive = TRUE;
	}
	else {
		interactive = FALSE;
	}
#ifdef INTERACTIVE
	interactive = TRUE;
#endif
	/* init */
	pool = p_create(8192);

	/* man loop */
	/* 1. initialize 
	 * 2. read commandline 
	 * 3. parse commandline
	 * 4. fork and execute
	 * 5. wait all childs
	 */
	for (;;) {
		/* clear and reinit */
		p_clear(pool);
		cmdline_buf_clear(&buf);
		cmdlist = l_create(pool);
		parser = create_parser(pool, cmdlist, &buf, env);

		/* read commandline and parse */
		if (interactive) {
			fputs(env->PS1, stderr);
		}
		retval = CMDLINE_PARSE_EMPTY;
		for (;;) {
			if (buf.space <= 1) {
				retval = CMDLINE_PARSE_TOKEN_TOO_LONG;
				break;
			}
			line = cmdline_buf_getline(&buf, instream);
			line_complete = cmdline_buf_line_complete(&buf);
			if (line == NULL) {
				if (buf.p == buf.data) {
					retval = CMDLINE_READING_TERMINATE;
				}
				else {
					retval = CMDLINE_READING_ERROR;
				}
				break;
			}

			retval = parser_parse(parser);
			if (retval != CMDLINE_PARSE_CONTINUE) {
				break;
			}
			if (interactive && line_complete) {
				fputs(env->PS2, stderr);
			}
		}
		if (retval == CMDLINE_READING_TERMINATE) {
			if (interactive) {
				fputc('\n', stdout);
			}
			break;
		}
		if (retval == CMDLINE_READING_ERROR) {
			fprintf(stderr, "%s: error reading commandline\n", argv[0]);
			continue;
		}
		if (retval == CMDLINE_PARSE_EMPTY) {
			fputc('\n', stdout);
			continue;
		}
		if (retval == CMDLINE_PARSE_TOKEN_TOO_LONG) {
			fprintf(stderr, "%s: error parsing commandline: token is too "
					"long\n", argv[0]);
			continue;
		}
		if (retval != CMDLINE_PARSE_DONE) {
			fprintf(stderr, "%s: %s\n", argv[0], errmsg(parser));
			continue;
		}

		for (node = cmdlist->first; node != NULL; node = node->next) {
			cmd = node->data;
#ifdef PRINT_COMMAND_ONLY
			print_command(cmd);
#else
			execute_command(pool, cmd);
#endif
		}
	}
	p_destroy(static_pool);
	p_destroy(pool);
	return 0;
}
