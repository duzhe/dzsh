#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "env.h"
#include "mempool.h"
#include "list.h"
#include "str.h"

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
		env->PS1 = "dzsh $ ";
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
	env->cwd->len = strlen(env->cwd->data);
	return 0;
}


