#ifndef _ENV_H_INCLUDE
#define _ENV_H_INCLUDE

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

int initialize_env(struct mempool *pool, int argc, char **argv);
#endif
