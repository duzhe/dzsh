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
#include "str.h"
#include "cmdline_parser.h"

void print_startup_infos(struct list *infos)
{
	struct process_startup_info *info;
	struct lnode *infonode;
	struct cstr *param;
	struct lnode *paramnode;
	struct redirection *re;
	struct lnode *renode;
	
	for (infonode = infos->first; infonode != NULL; infonode = infonode->next) {
		info = infonode->data;
		for (paramnode = info->params->first; paramnode != NULL; paramnode = paramnode->next) {
			param = paramnode->data;
			cstr_print(param, stdout);
			fputc(' ', stdout);
		}
		for (renode = info->redirections->first; renode != NULL; renode = renode->next) {
			re = renode->data;
			switch (re->flags) {
				case 0:
					printf("fd%d->fd%d ", re->leftfd, re->right.fd);
					break;
				case 1:
					printf("fd%d<-fd%d ", re->right.fd, re->leftfd);
					break;
				case 2:
					printf("fd%d->", re->leftfd);
					cstr_print(re->right.pathname, stdout);
					fputc(' ', stdout);
					break;
				case 3:
					printf("fd%d<-", re->leftfd);
					cstr_print(re->right.pathname, stdout);
					fputc(' ', stdout);
					break;
				case 6:
					printf("fd%d->>", re->leftfd);
					cstr_print(re->right.pathname, stdout);
					fputc(' ', stdout);
					break;
				default:
					printf("unexpected flags %d\n", re->flags);
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
	const char *IFS;
	struct mempool *pool;
	struct list *cmdlist;
	struct list *process_startup_infos;
	struct lnode *node;
	struct cmdline_parser *parser;
	int i;
	struct cmdline_buf buf;
	int len;
    char *testcases[] = {
               "ls\n",
               "ls -l\n",
               "ls 'a c'\n",
               "ls -a > /tmp/aaa\n",
               "ls -l -a >> /tmp/aaa\n",
               "sort < /tmp/aaa > /tmp/bbb\n",
               "ls -la >> /tmp/aaa 2>&1\n",
               "ls -l 2> /dev/null | sort -k1 \n",
               "echo -n hello > /tmp/ttt 2>&1\n",
               "cat /tmp/ttt | sort -k2 -nr | uniq -c > /tmp/sorted\n",
               "cat /tmp/ttt | sort -k2 -nr ; ls > /tmp/ttt\n",
   };


	IFS = " \t\n";
	pool = p_create(8192);

	for (i=0; i< sizeof(testcases)/sizeof(*testcases); ++i) {
		p_clear(pool);
		cmdline_buf_clear(&buf);
		cmdlist = l_create(pool);
		len = strlen(testcases[i]);
		memcpy(buf.data, testcases[i], len+1);
		buf.p += len;
		buf.space -= len;
		parser = create_cmdline_parser(pool,  cmdlist, &buf, IFS);

		printf("orig cmdline:\n\t%s", testcases[i]);
		/* parse commandline */
		if (cmdline_parse(parser) != CMDLINE_PARSE_DONE) {
			fprintf(stderr, "%s: %s\n", argv[0], errmsg(parser));
			/*
			continue;
			*/
		}
		printf("parse result:\n");
		for (node = cmdlist->first; node != NULL; node = node->next) {
			process_startup_infos = node->data;
			printf("\t");
			print_startup_infos(process_startup_infos);
		}
		printf("\n");
	}
	return 0;
}
