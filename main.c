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


static const char *program;


int do_redirect(struct process_startup_info *info)
{
	struct redirection_pair *p;
	char buf[1024];
	int fromfd;
	int tofd;
	int retval;
	const char *filename;
	struct lnode *node;
	for (node = info->redirections->first; node != NULL; node = node->next) {
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
				fprintf(stderr, "%s: cannot open file %s for write: %s\n", program, 
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
				fprintf(stderr, "%s: cannot open file %s for read: %s\n", program, 
						p->from.pathname, strerror(errno));
				return -1;
			}
		}
		else {
			fromfd = p->from.fd;
		}
		retval = dup2(tofd, fromfd);
		if (retval == -1) {
			fprintf(stderr, "%s: cannot redirect file: %s\n", program, 
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


int main(int argc, char **argv)
{
	char buf[1024];
	char binbuf[1024];
	char pathbuf[4096];
	char *pathentry[32];
	char *bin;
	char *line;
	const char *errmsg;
	const char *pathname;
	char *tok;
	int i;
	int retval;
	struct stat statbuf;
	const char *IFS;
	const char *PS1;
	const char *PATH;
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
	struct list *sonpids;
	struct lnode *pidnode;
	struct mempool *pool;
	char **params;
	size_t paramscount;
	struct lnode *paramsnode;
	struct cmdline_parser *parser;

	IFS = getenv("IFS");
	if (IFS == NULL) {
		IFS = " \t\n";
	}
	PS1 = getenv("PS1");
	if (PS1 == NULL) {
		PS1 = "dzsh $";
	}
	PATH = getenv("PATH");

	/* parse dzsh commandline */
	program = argv[0];
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

	/* init */
	pool = p_create(8192);

	/* init path entry */
	i = 0;
	strncpy(pathbuf, PATH, sizeof(pathbuf));
	tok = strtok(pathbuf, ":");
	pathentry[i++] = tok;
	while (tok != NULL) {
		tok = strtok(NULL, ":");
		pathentry[i++] = tok;
	}

	/* man loop */
	while (1) {
		/* clear and reinit */
		p_clear(pool);
		process_startup_infos = l_create(pool);
		parser = create_cmdline_parser(pool,  process_startup_infos, buf, IFS);

		/* put PS1 */
		fputs(PS1, stdout);

		/* read commandline */
		line = fgets(buf, sizeof(buf), instream);
		if (line == NULL) {
			fputc('\n', stdout);
			break;
		}

		/* parse commandline */
		if (cmdline_parse(parser, &errmsg) == -1) {
			fprintf(stderr, "%s: %s\n", program, errmsg);
			continue;
		}
		infonode = process_startup_infos->first;
		if (infonode != NULL) {
			info = infonode->data;
			if (info->params->first != NULL) {
				bin = info->params->first->data;
			}
		}
		else {
			bin = NULL;
		}

		/* parse bin path */
		if (bin != NULL) switch(*bin) {
		case '/':
			break;
		case '.':
			if (getcwd(binbuf, sizeof(binbuf)) == NULL) {
				fprintf(stderr, "%s: fail to getcwd: %s\n", argv[0],
						strerror(errno));
				continue;
			}
			strncat(binbuf, "/", sizeof(binbuf));
			strncat(binbuf, bin, sizeof(binbuf));
			bin = binbuf;
			break;
		default:
			for (i=0; i< sizeof(pathentry)/sizeof(*pathentry); ++i) {
				if (pathentry[i] == NULL) {
					break;
				}
				strncpy(binbuf, pathentry[i], sizeof(binbuf));
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
			if (*bin != '/') {
				fprintf(stderr, "cannot find %s in PATH ENVIREMENT: %s\n",
						bin, PATH);
				continue;
			}
			break;
		}

		fdin = STDIN_FILENO;
		fdout = STDOUT_FILENO;
		sonpids = l_create(pool);
		while (infonode != NULL) {
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

			/* fork and execute */
			pid = fork();
			if (pid == -1) {
				fprintf(stderr, "%s\n", strerror(errno));
			}
			if (pid == 0) {
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
				if (do_redirect(info) == -1) {
					return 1;
				}
				paramscount = l_count(info->params);
				if (paramscount != 0) {
					params = p_alloc(pool, sizeof(char*) *(paramscount+1));
					i = 0;
					for (paramsnode = info->params->first; paramsnode != NULL;
							paramsnode = paramsnode->next) {
						params[i++] = paramsnode->data;
					}
					params[i] = NULL;
					bin = params[0];
					execvp(bin, params);
				}
				return 0;
			}
			else {
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
				infonode = infonode->next;
			}
		}
		/* after the last fork, wait for all son process */
		for (pidnode = sonpids->first; pidnode != NULL; 
				pidnode = pidnode->next) {
			pidpointer = pidnode->data;
			waitpid(*pidpointer, &childstatus, 0);
		}
	}
	return 0;
}
