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
#include "cmdline_parser.h"


struct env {
	struct mempool *pool;
	int argc;
	char **argv;
	const char *IFS;
	const char *PS1;
	const char *PS2;
	const char *PATH;
	struct list *pathentry;
	long maxpath;
	struct cstr *cwd;
}*env;


int initialize_env(struct mempool *pool, int argc, char **argv)
{
	struct list *pathentry;
	char *ppathbuf;
	char *tok;
	env = p_alloc(pool, sizeof(struct env));
	env->pool = pool;
	env->argc = argc;
	env->argv = argv;

	env->IFS = getenv("IFS");
	if (env->IFS == NULL) {
		env->IFS = " \t\n";
	}

	env->PS1 = getenv("PS1");
	if (env->PS1 == NULL) {
		env->PS1 = "dzsh $";
	}
	
	env->PS2 = getenv("PS2");
	if (env->PS2 == NULL) {
		env->PS2 = "> ";
	}

	env->PATH = getenv("PATH");

	ppathbuf = p_strdup(pool, env->PATH);
	pathentry = l_create(pool);
	tok = strtok(ppathbuf, ":");
	while (tok != NULL) {
		l_pushback(pathentry, make_cstr(pool, tok, strlen(tok)));
		tok = strtok(NULL, ":");
	}
	env->pathentry = pathentry;

#ifdef _PC_PATH_MAX
	env->maxpath = pathconf("/", _PC_PATH_MAX);
	if (env->maxpath == -1) {
		env->maxpath = 4096;
	}
#else
	env->maxpath = 4096;
#endif
	env->cwd = new_cstr(pool);
	env->cwd->data = p_large_alloc(pool, env->maxpath);
	if (getcwd((char*)env->cwd->data, env->maxpath) == NULL) {
		fprintf(stderr, "%s: cannot get current working directory: %s", 
				env->argv[0], strerror(errno));
		return -1;
	}
	return 0;
}


const char *getfullpathname(struct mempool *pool, struct cstr *name)
{
	struct cstr *cwd;
	char *pathname;
	int len;
	if (*(name->data) == '/') {
		return p_cstrdup(pool, name);
	}
	cwd = env->cwd;
	len = cwd->len + 1 + name->len;
	pathname = p_alloc(pool, len+1);
	memcpy(pathname, cwd->data, cwd->len);
	pathname[cwd->len] = '/';
	memcpy(pathname+ cwd->len +1, name->data, name->len);
	return pathname;
}


int do_redirect(struct list *redirections)
{
	struct mempool *pool;
	struct redirection *p;
	int fromfd;
	int tofd;
	int retval;
	const char *filename;
	struct lnode *node;
	int rightfd;
	const char *openfor;
	pool = p_create(8196);
	for (node = redirections->first; node != NULL; node = node->next) {
		p = node->data;
		if (p->flags & REDIRECT_FILE) {
			filename = getfullpathname(pool, p->right.pathname);
			if (p->flags & REDIRECT_IN) {
				rightfd = open(filename,  O_RDONLY);
				openfor = "read";
			}
			else {
				if (p->flags & REDIRECT_APPEND) {
					rightfd = open(filename, O_APPEND|O_WRONLY);
				}
				else {
					rightfd = open(filename, O_CREAT|O_TRUNC|O_WRONLY,
							S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
				}
				openfor = "write";
			}
			if (rightfd == -1) {
				fprintf(stderr, "%s: cannot open file ", env->argv[0]);
				cstr_print(p->right.pathname, stderr);
				fprintf(stderr, " for %s: %s\n", openfor, strerror(errno));
				return -1;
			}
		}
		else {
			rightfd = p->right.fd;
		}

		if (p->flags & REDIRECT_IN) {
			fromfd = rightfd;
			tofd = p->leftfd;
		}
		else {
			fromfd = p->leftfd;
			tofd = rightfd;
		}
		retval = dup2(tofd, fromfd);
		if (retval == -1) {
			fprintf(stderr, "%s: cannot redirect file: %s\n", env->argv[0], 
					strerror(errno));
			return -1;
		}
		if (p->flags & REDIRECT_FILE) {
			close(rightfd);
		}
	}
	return 0;
}


/*
void dbgout(struct process_startup_info *info)
{
	int pid;
	int ppid;
	struct cstr *bin;
	pid = getpid();
	ppid = getppid();
	bin = info->params->first->data;
	fprintf(stderr, "process start: pid: %d,  ppid: %d, bin:%s\n",
			pid, ppid, bin);
}
*/


static int subprocess_exec(const char *bin, char **params, struct list *redirections, int fdin, int fdout)
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
	if (do_redirect(redirections) == -1) {
		return -1;
	}
	if (bin != NULL) {
		retval = execv(bin, params);
		if (retval == -1) {
			return 1;
		}
	}
	return 0; 
}


static const char *getbinpathname(struct mempool *pool, struct cstr *bin, struct list *pathentry)
{
	struct lnode *node;
	size_t len;
	char *pathname;
	struct cstr *binpath;
	struct stat statbuf;
	int retval;
	if (cstr_empty(bin)) {
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


static BOOL check(struct mempool *pool, struct list *infos) 
{
	struct lnode *infonode;
	struct process_startup_info *info;
	struct cstr *bin;
	const char *binfullpath;
	BOOL all_ok;
	/* check commandline */
	all_ok = TRUE;
	for (infonode = infos->first; infonode!=NULL; infonode=infonode->next) {
		if (infonode == NULL) {
			continue;
		}
		info = infonode->data;
		if (info->params->first != NULL) {
			bin = info->params->first->data;
			binfullpath = getbinpathname(pool, bin, env->pathentry);
			if (binfullpath == NULL) {
				all_ok = FALSE;
				break;
			}
		}
		else {
			binfullpath = NULL;
		}
		info->bin = binfullpath;
	}
	return all_ok;
}


int execute(struct mempool *pool, struct list *process_startup_infos)
{
	int fdin, fdout;
	struct list *sonpids;
	struct lnode *infonode;
	struct lnode *paramsnode;
	struct lnode *pidnode;
	struct process_startup_info *info;
	int pipefd[2];
	int paramscount;
	char **params;
	int i;
	int childstatus;
	pid_t pid;
	pid_t *pidpointer;
	int retval;

	fdin = STDIN_FILENO;
	fdout = STDOUT_FILENO;
	sonpids = l_create(pool);
	/* each iteration produce a subprocess */
	/* 1. prepare startup info and pipefds 
	 * 2. fork  child process exec into specified command
	 *          parent save child pid and go to next iteration
	 * */
	for (infonode = process_startup_infos->first; infonode != NULL; 
			infonode = infonode->next) {
		/* prepare pipes */
		/* pipefd[1] is for write , using it as STDOUT at the previous 
		 * process (on the left side of '|' sign), pipefd[0] is for read, 
		 * using it as STDIN at the next process (on the right side of '|'
		 * sign)
		 * first process, using STDIN as STDIN.
		 * the last process , using STDOUT as STDOUT.
		 */
		info = infonode->data;
		if (infonode->next != NULL) {
			retval = pipe(pipefd);
			if (retval == -1) {
				fprintf(stderr, "%s: fail create pipe: %s\n", env->argv[0], 
						strerror(errno));
			}
			fdout = pipefd[1];
		}
		else {
			fdout = STDOUT_FILENO;
		}

		/* get an array form 'params' */
		paramscount = l_count(info->params);
		params = p_alloc(pool, sizeof(char*) *(paramscount+1));
		i = 0;
		for (paramsnode = info->params->first; paramsnode != NULL;
				paramsnode = paramsnode->next) {
			params[i++] = p_cstrdup(pool, paramsnode->data);
		}
		params[i] = NULL;

		/* fork and execute */
		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			break;
		}
		if (pid == 0) {
			retval = subprocess_exec(info->bin, params, info->redirections, fdin, fdout);
			exit(retval);
		}
		/* save son process pid */
		pidpointer = p_alloc(pool, sizeof(pid_t));
		*pidpointer = pid;
		pidnode = l_pushback(sonpids, pidpointer);

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
	for (pidnode = sonpids->first; pidnode != NULL; 
			pidnode = pidnode->next) {
		pidpointer = pidnode->data;
		waitpid(*pidpointer, &childstatus, 0);
	}
	return 0;
}


int main(int argc, char **argv)
{
	struct mempool *static_pool;
	char buf[1024];
	char *pbuf;
	char *line;
	size_t len;
	const char *pathname;
	int retval;
	FILE *instream;
	struct list *cmdlist;
	struct list *process_startup_infos;
	struct mempool *pool;
	struct cmdline_parser *parser;
	struct lnode *node;

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
		pathname = getfullpathname(static_pool, dup_cstr(static_pool, argv[1])); 
		instream = fopen(pathname, "r");
		if (instream == NULL) {
			fprintf(stderr, "%s: cannot access file %s: %s\n", argv[0], 
					argv[1], strerror(errno));
			return 1;
		}
	}

	/* init */
	pool = p_create(8192);

	/* man loop */
	/* 1. initialize 
	 * 2. read commandline 
	 * 3. parse commandline
	 * 4. check commanline
	 * 5. fork and execute
	 * 6. wait all childs
	 */
	for (;;) {
		/* clear and reinit */
		p_clear(pool);
		/*
		process_startup_infos = l_create(pool);
		parser = create_cmdline_parser(pool,  process_startup_infos, buf, env->IFS);
		*/
		cmdlist = l_create(pool);
		parser = create_cmdline_parser(pool,  cmdlist, buf, env->IFS);

		/* read commandline and parse */
		fputs(env->PS1, stdout);
		pbuf = buf;
		retval = CMDLINE_PARSE_EMPTY;
		for (;;) {
			line = fgets(pbuf, sizeof(buf) - (pbuf-buf), instream);
			if (line == NULL) {
				if (pbuf == buf) {
					retval = CMDLINE_READING_TERMINATE;
				}
				else {
					retval = CMDLINE_READING_ERROR;
				}
				break;
			}
			len = strlen(pbuf);
			pbuf += len;

			retval = cmdline_parse(parser);
			if (retval == CMDLINE_PARSE_CONTINUE) {
				fputs(env->PS2, stdout);
				continue;
			}
			break;
		}
		if (retval == CMDLINE_READING_TERMINATE) {
			fputc('\n', stdout);
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
		if (retval != CMDLINE_PARSE_DONE) {
			fprintf(stderr, "%s: %s\n", argv[0], errmsg(parser));
			continue;
		}

		for (node = cmdlist->first; node != NULL; node = node->next) {
			process_startup_infos = node->data;
			if (!check(pool, process_startup_infos)){
				continue;
			}
			execute(pool, process_startup_infos);
		}
	}
	p_destroy(static_pool);
	p_destroy(pool);
	return 0;
}
