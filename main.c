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


#define REDIRECT_FROM_FILE (0x01)
#define REDIRECT_TO_FILE (0x02)
#define REDIRECT_APPEND (0x04)
#define REDIRECT_SENTINEL (-1)
typedef struct redirection_pair{
	int flags;
	union {
		int fd;
		char *pathname;
	} from;
	union {
		int fd;
		char *pathname;
	} to;
}redirection_pair;

int isnumeric(const char *p)
{
	while (*p !=0) {
		if (!isdigit(*p++)) {
			return 0;
		}
	}
	return 1;
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

const char *IFS;
char buf[1024];
char *params[256];
struct redirection_pair redirections[16];
const char *program;


#define PARSE_STATE_NORMAL 0
/*
#define PARSE_STATE_QUATA 1
#define PARSE_STATE_DAUQTA 2
*/
#define PARSE_STATE_REDIRECT_TO 3
#define PARSE_STATE_REDIRECT_FROM 4
int parse_commandline()
{
	int state;
	char *tok, *p;
	int iparams, iredi;
	state = PARSE_STATE_NORMAL;
	iparams = 0;
	iredi = 0;
	
	p = buf;
	tok = buf;
	for (p=buf; *p != '\0'; ++p) {
		switch (*p) {
		case '<':
			*p = '\0';
			redirections[iredi].flags = 0;
			if (p == tok) {
				redirections[iredi].to.fd = STDIN_FILENO;
			}
			else if (isnumeric(tok)) {
				redirections[iredi].to.fd = atoi(tok);
			}
			else {
				params[iparams++] = tok;
				redirections[iredi].to.fd = STDIN_FILENO;
			}
			tok = p+1;
			state = PARSE_STATE_REDIRECT_FROM;
			continue;
		case '>':
			*p = '\0';
			redirections[iredi].flags = 0;
			if (p == tok) {
				redirections[iredi].from.fd = STDOUT_FILENO;
			}
			else if (isnumeric(tok)) {
				redirections[iredi].from.fd = atoi(tok);
			}
			else {
				params[iparams++] = tok;
				redirections[iredi].from.fd = STDOUT_FILENO;
			}
			if (p[1] == '>') {
				redirections[iredi].flags |= REDIRECT_APPEND;
				p[1] = *IFS;
			}
			tok = p+1;
			state = PARSE_STATE_REDIRECT_TO;
			continue;
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
				params[iparams++] = tok;
				tok = p+1;
				break;
			case PARSE_STATE_REDIRECT_TO:
				if (*tok == '&') {
					if (!isnumeric(tok+1)) {
						fprintf(stderr, "not a valid file discriptor\n");
						return -1;
					}
					redirections[iredi].to.fd = atoi(tok+1);
				}
				else {
					redirections[iredi].flags |= REDIRECT_TO_FILE;
					redirections[iredi].to.pathname = tok;
				}
				++iredi;
				tok = p+1;
				state = PARSE_STATE_NORMAL;
				break;
			case PARSE_STATE_REDIRECT_FROM:
				if (*tok == '&') {
					if (!isnumeric(tok+1)) {
						fprintf(stderr, "not a valid file discriptor\n");
						return -1;
					}
					redirections[iredi].from.fd = atoi(tok+1);
				}
				else {
					redirections[iredi].flags |= REDIRECT_FROM_FILE;
					redirections[iredi].from.pathname = tok;
				}
				++iredi;
				tok = p+1;
				state = PARSE_STATE_NORMAL;
				break;
			default:
				fprintf(stderr, "unexpected position\n");
				break;
			}
		}
	}
	params[iparams] = NULL;
	redirections[iredi].flags = REDIRECT_SENTINEL;
	return 0;
}

int do_redirect()
{
	struct redirection_pair *p;
	char buf[1024];
	int fromfd;
	int tofd;
	int retval;
	const char *filename;
	for (p = redirections; p->flags != REDIRECT_SENTINEL; ++p) {
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

int main(int argc, char **argv)
{
	char binbuf[1024];
	char pathbuf[4096];
	char *pathentry[32];
	char *bin;
	char *line;
	const char *pathname;
	char *tok;
	int i;
	int retval;
	struct stat statbuf;
	const char *PS1;
	const char *PATH;
	int pid;
	int childstatus;
	FILE *instream;

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
		/* put PS1 */
		fputs(PS1, stdout);

		/* read commandline */
		line = fgets(buf, sizeof(buf), instream);
		if (line == NULL) {
			fputc('\n', stdout);
			break;
		}

		/* parse commandline */
		if (parse_commandline(params, buf) == -1) {
			continue;
		}
		bin = params[0];
		
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

		/* fork and execute */
		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
		}
		if (pid == 0) {
			if (do_redirect() == -1) {
				return 1;
			}
			if (bin != NULL) {
				execv(bin, params);
			}
			return 0;
		}
		else {
			wait(&childstatus);
		}
	}
	return 0;
}
