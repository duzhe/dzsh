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

void print_startup_infos(struct list *infos)
{
	struct process_startup_info *info;
	struct lnode *infonode;
	const char *param;
	struct lnode *paramnode;
	struct redirection_pair *re;
	struct lnode *renode;
	
	for (infonode = infos->first; infonode != NULL; infonode = infonode->next) {
		info = infonode->data;
		for (paramnode = info->params->first; paramnode != NULL; paramnode = paramnode->next) {
			param = paramnode->data;
			printf("%s ", param);
		}
		for (renode = info->redirections->first; renode != NULL; renode = renode->next) {
			re = renode->data;
			switch (re->flags) {
				case 0:
					printf("fd%d->fd%d ", re->from.fd, re->to.fd);
					break;
				case 1:
					printf("%s->fd%d ", re->from.pathname, re->to.fd);
					break;
				case 2:
					printf("fd%d->%s ", re->from.fd, re->to.pathname);
					break;
				case 6:
					printf("fd%d->>%s ", re->from.fd, re->to.pathname);
					break;
			}
		}
		if (infonode->next != NULL) {
			printf(" | ");
		}
	}
	printf("\n");
}

int main(int argc, char **argv)
{
	const char *errmsg;
	const char *IFS;
	char *line;
	struct mempool *pool;
	struct list *process_startup_infos;
	struct cmdline_parser *parser;
	int i;
	char *testcases[] = {
		"ls\n",
		"ls -l\n",
		"ls -a > /tmp/aaa\n",
		"ls -l -a >> /tmp/aaa\n",
		"sort < /tmp/aaa > /tmp/bbb\n",
		"ls -la >> /tmp/aaa 2>&1\n",
		"ls -l 2> /dev/null | sort -k1 \n",
		"echo -n hello > /tmp/ttt 2>&1\n",
		"cat /tmp/ttt | sort -k2 -nr | uniq -c > /tmp/sorted\n"};



	IFS = " \t\n";
	pool = p_create(8192);

	for (i=0; i< sizeof(testcases)/sizeof(*testcases); ++i) {
		p_clear(pool);
		process_startup_infos = l_create(pool);
		line = p_strdup(pool, testcases[i]);
		parser = create_cmdline_parser(pool,  process_startup_infos, line, IFS);

		printf("orig cmdline:\n\t%s", testcases[i]);
		/* parse commandline */
		if (cmdline_parse(parser, &errmsg) == -1) {
			fprintf(stderr, "%s: %s\n", argv[0], errmsg);
			/*
			continue;
			*/
		}
		printf("parse result:\n\t");
		print_startup_infos(process_startup_infos);
		printf("\n");
	}
	return 0;
}
