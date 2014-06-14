#ifndef _CMDLINE_BUF_H_INCLUDE
#define _CMDLINE_BUF_H_INCLUDE
#include <stdio.h>
#include "bool.h"

#ifndef READBUFSIZE 
#define READBUFSIZE 8192
#endif

#if READBUFSIZE < 16
#define READBUF 16
#endif

struct cmdline_buf {
	char data[READBUFSIZE];
	char *p;
	size_t space;
};

void cmdline_buf_clear(struct cmdline_buf *);
const char *cmdline_buf_getline(struct cmdline_buf *, FILE* infile);
void cmdline_buf_parsed(struct cmdline_buf *, const char *parsed);
BOOL cmdline_buf_line_complete(struct cmdline_buf *);

#endif
