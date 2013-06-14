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
#include "mempool.h"
#include "list.h"
#include "cmdline_parser.h"


struct env {
	struct mempool *pool;
	int argc;
	const char **argv;
	const char *IFS;
	const char *PS1;
	const char *PATH;
	struct list *pathentry;
	/*
	const char *cwd;
	*/
}*env;


int initialize_env(struct mempool *pool)
{
	struct list *pathentry;
	char *ppathbuf;
	char *tok;
	env = p_alloc(pool, sizeof(struct env));
	env->pool = pool;

	env->IFS = getenv("IFS");
	if (env->IFS == NULL) {
		env->IFS = " \t\n";
	}

	env->PS1 = getenv("PS1");
	if (env->PS1 == NULL) {
		env->PS1 = "dzsh $";
	}

	env->PATH = getenv("PATH");

	ppathbuf = p_strdup(pool, env->PATH);
	pathentry = l_create(pool);
	tok = strtok(ppathbuf, ":");
	while (tok != NULL) {
		l_pushback(pathentry, tok);
		tok = strtok(NULL, ":");
	}
	env->pathentry = pathentry;
	return 0;
}


const char *getfullpathname(char *pbuf, size_t bufsize, char *name)
{
	if (*name == '/') {
		return name;
	}
	if (getcwd(pbuf, bufsize) == NULL) {
		return NULL;
	}
	strncat(pbuf, "/", bufsize);
	strncat(pbuf, name, bufsize);
	return pbuf;
}


int do_redirect(struct list *redirections)
{
	struct redirection_pair *p;
	char buf[1024];
	int fromfd;
	int tofd;
	int retval;
	const char *filename;
	struct lnode *node;
	for (node = redirections->first; node != NULL; node = node->next) {
		p = node->data;
		if (p->flags & REDIRECT_TO_FILE) {
			filename = getfullpathname(buf, sizeof(buf), p->to.pathname);
			if (p->flags & REDIRECT_APPEND) {
				tofd = open(filename, O_APPEND|O_WRONLY);
			}
			else {
				tofd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, 
						S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			}
			if (tofd == -1) {
				fprintf(stderr, "%s: cannot open file %s for write: %s\n", env->argv[0], 
						p->to.pathname, strerror(errno));
				return -1;
			}
		}
		else {
			tofd = p->to.fd;
		}
		if (p->flags & REDIRECT_FROM_FILE) {
			filename = getfullpathname(buf, sizeof(buf), p->from.pathname);
			fromfd = open(filename, O_RDONLY);
			if (fromfd == -1) {
				fprintf(stderr, "%s: cannot open file %s for read: %s\n", env->argv[0], 
						p->from.pathname, strerror(errno));
				return -1;
			}
		}
		else {
			fromfd = p->from.fd;
		}
		retval = dup2(tofd, fromfd);
		if (retval == -1) {
			fprintf(stderr, "%s: cannot redirect file: %s\n", env->argv[0], 
					strerror(errno));
			return -1;
		}
		if (p->flags & REDIRECT_TO_FILE && tofd != fromfd) {
			close(tofd);
		}
		if (p->flags & REDIRECT_FROM_FILE && tofd != fromfd) {
			close(fromfd);
		}
	}
	return 0;
}

void dbgout(struct process_startup_info *info)
{
	int pid;
	int ppid;
	const char *bin;
	pid = getpid();
	ppid = getppid();
	bin = info->params->first->data;
	fprintf(stderr, "process start: pid: %d,  ppid: %d, bin:%s\n",
			pid, ppid, bin);
}


int start_subprocess(char **params, struct list *redirections, int fdin, int fdout)
{
	/*
	dbgout(info);
	*/
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
	if (params[0] != NULL) {
		execvp(params[0], params);
	}
	return 0;
}


int check_command(const char *bin, struct list *pathentry)
{
	char binbuf[1024];
	struct lnode *node;
	int retval;
	struct stat statbuf;
	if (bin == NULL) {
		return 0;
	}
	switch(*bin) {
	case '/':
		break;
	case '.':
		if (getcwd(binbuf, sizeof(binbuf)) == -1) {
			/*
			fprintf(stderr, "%s: fail to getcwd: %s\n", argv[0],
					strerror(errno));
			*/
			return -1;;
		}
		strncat(binbuf, "/", sizeof(binbuf));
		strncat(binbuf, bin, sizeof(binbuf));
		bin = binbuf;
		break;
	default:
		for (node=pathentry->first; node!=NULL; node=node->next) {
			if (node->data == NULL) {
				break;
			}
			strncpy(binbuf, node->data, sizeof(binbuf));
			strncat(binbuf, "/", sizeof(binbuf));
			strncat(binbuf, bin, sizeof(binbuf));
			retval = stat(binbuf, &statbuf);
			if (retval != 0) {
				continue;
			}
			if (statbuf.st_mode & S_IFREG) {
				bin = binbuf;
				break;
			}
		}
		break;
	}
	if (*bin != '/') {
		return -1;
	}
	return 0;
}


int main(int argc, char **argv)
{
	struct mempool *static_pool;
	char buf[1024];
	char *bin;
	char *line;
	const char *errmsg;
	const char *pathname;
	int i;
	int all_ok;
	int retval;
	pid_t pid;
	pid_t *pidpointer;
	int childstatus;
	FILE *instream;
	struct list *process_startup_infos;
	struct process_startup_info *info;
	struct lnode *infonode;
	int pipefd[2];
	int fdin;
	int fdout;
	struct mempool *pool;
	struct list *sonpids;
	struct lnode *pidnode;
	char **params;
	size_t paramscount;
	struct lnode *paramsnode;
	struct cmdline_parser *parser;

	/* parse dzsh commandline */
	if (argc == 1) {
		instream = stdin;
	}
	else {
		if (argc > 2) {
			fprintf(stderr, "Warning: accept one argument only," \
					"more arguments will be ignored\n");
		}
		pathname = getfullpathname(buf, sizeof(buf), argv[1]); 
		instream = fopen(pathname, "r");
		if (instream == NULL) {
			fprintf(stderr, "%s: cannot access file %s: %s\n", argv[0], 
					argv[1], strerror(errno));
			return 1;
		}
	}

	/* global initialize */
	static_pool = p_create(8196);
	if (initialize_env(static_pool) != 0) {
		return -1;
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
	while (1) {
		/* clear and reinit */
		p_clear(pool);
		process_startup_infos = l_create(pool);
		parser = create_cmdline_parser(pool,  process_startup_infos, buf, env->IFS);

		/* put PS1 and read commandline */
		fputs(env->PS1, stdout);
		line = fgets(buf, sizeof(buf), instream);
		if (line == NULL) {
			fputc('\n', stdout);
			break;
		}

		/* parse commandline */
		if (cmdline_parse(parser, &errmsg) == -1) {
			fprintf(stderr, "%s: %s\n", env->argv[0], errmsg);
			continue;
		}

		/* check commandline */
		all_ok = 1;
		for (infonode = process_startup_infos->first; infonode!=NULL;
				infonode=infonode->next) {
			if (infonode == NULL) {
				continue;
			}
			info = infonode->data;
			if (info->params->first != NULL) {
				bin = info->params->first->data;
			}
			retval = check_command(bin, env->pathentry);
			if (retval != 0) {
				fprintf(stderr, "cannot find %s in PATH ENVIREMENT: %s\n",
						bin, env->PATH);
				all_ok = 0;
				break;
			}
		}
		if (!all_ok) {
			continue;
		}

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
				pipe(pipefd);
				fdout = pipefd[1];
			}
			else {
				fdout = STDOUT_FILENO;
			}

			/* get an array form 'params' */
			paramscount = l_count(info->params);
			if (paramscount != 0) {
				params = p_alloc(pool, sizeof(char*) *(paramscount+1));
				i = 0;
				for (paramsnode = info->params->first; paramsnode != NULL;
						paramsnode = paramsnode->next) {
					params[i++] = paramsnode->data;
				}
				params[i] = NULL;
			}

			/* fork and execute */
			pid = fork();
			if (pid == -1) {
				fprintf(stderr, "%s\n", strerror(errno));
				break;
			}
			if (pid == 0) {
				return start_subprocess(params, info->redirections, fdin, fdout);
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
	}
	p_destroy(static_pool);
	p_destroy(pool);
	return 0;
}
