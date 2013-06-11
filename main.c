#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int main(int argc, char **argv)
{
	char buf[1024];
	char binbuf[1024];
	char pathbuf[4096];
	char *pathentry[32];
	char *line;
	char *tok;
	char *bin;
	char *params[256];
	int i;
	int retval;
	struct stat statbuf;
	const char *IFS;
	const char *PS1;
	const char *PATH;
	int pid;

	IFS = getenv("IFS");
	if (IFS == NULL) {
		IFS = " \t\n";
	}
	PS1 = getenv("PS1");
	PATH = getenv("PATH");

	/* init path entry */
	i = 0;
	strncpy(pathbuf, PATH, sizeof(pathbuf));
	tok = strtok(pathbuf, ":");
	pathentry[i++] = tok;
	while (tok != NULL ) {
		tok = strtok(NULL, ":");
		pathentry[i++] = tok;
	}

	/* man loop */
	while (1) {
		/* put PS1 */
		fputs("$", stdout);

		/* read commandline */
		line = fgets(buf, sizeof(buf), stdin);
		if (line == NULL) {
			break;
		}

		/* parse commandline */
		i = 0;
		bin = strtok(buf, IFS);
		if (bin == NULL) {
			continue;
		}
		tok = bin;
		params[i++] = tok;
		while (tok != NULL) {
			tok = strtok(NULL, IFS);
			params[i++] = tok;
		};
		
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
				if (pathentry[i] == NULL ) {
					break;
				}
				strncpy(binbuf, pathentry[i], sizeof(binbuf));
				strncat(binbuf, "/", sizeof(binbuf));
				strncat(binbuf, bin, sizeof(binbuf));
				retval = stat(binbuf, &statbuf);
				if (retval != 0 ) {
					continue;
				}
				if (statbuf.st_mode & S_IFREG ) {
					bin = binbuf;
					break;
				}
			}
			if (*bin != '/' ) {
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
