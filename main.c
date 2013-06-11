#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

const char *getfullpathname(char *pbuf, size_t bufsize, char *name)
{
	if (*name == '/') {
		return name;
	}
	getcwd(pbuf, bufsize);
	strncat(pbuf, "/", bufsize);
	strncat(pbuf, name, bufsize);
	return pbuf;
}

const char *IFS;
char buf[1024];
char *params[256];

#define PARSE_STATE_NORMAL 0
#define PARSE_STATE_QUATA 1
#define PARSE_STATE_DAUQTA 2
int parse_commandline()
{
	/*
	int state;
	char *pline;
	char *presult[];
	state = PARSE_STATE_NORMAL;
	pline = line;
	presult = result
	while (true) {
		switch (*pline) {
		case '\':
			break;
			case 
		}
	}
	*/
	char *tok;
	int i;
	i = 0;
	tok = strtok(buf, IFS);
	if (tok == NULL) {
		return -1;
	}
	params[i++] = tok;
	while (tok != NULL) {
		tok = strtok(NULL, IFS);
		params[i++] = tok;
	};
	return 0;

}

int main(int argc, char **argv)
{
	char escapebuf[1024];
	char binbuf[1024];
	char pathbuf[4096];
	char *pathentry[32];
	char *line;
	const char *pathname;
	char *tok;
	char *bin;
	int i;
	int retval;
	struct stat statbuf;
	const char *PS1;
	const char *PATH;
	int pid;
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

	/* parse commandline */
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
		/*
		i = 0;
		tok = strtok(buf, IFS);
		if (tok == NULL) {
			continue;
		}
		params[i++] = tok;
		while (tok != NULL) {
			tok = strtok(NULL, IFS);
			params[i++] = tok;
		};
		bin = params[0];
		*/
		
		/* parse bin path */
		switch(*bin) {
		case '/':
			break;
		case '.':
			getcwd(binbuf, sizeof(binbuf));
			strcat(binbuf, "/");
			strcat(binbuf, bin);
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
			execv(bin, params);
		}
		else {
			wait();
		}
	}
	return 0;
}
