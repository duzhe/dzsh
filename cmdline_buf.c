#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "cmdline_parser.h"

void cmdline_buf_clear(struct cmdline_buf *buf)
{
	buf->p = buf->data;
	buf->space = sizeof(buf->data);
}


const char *cmdline_buf_getline(struct cmdline_buf *buf, FILE *infile)
{
	int len;
	const char * line;
	line = fgets(buf->p, buf->space, infile);
	if (line != NULL) {
		len = strlen(line);
		buf->p += len;
		buf->space -= len;
	}
	return line;
}


void cmdline_buf_parsed(struct cmdline_buf *buf, const char *parsed)
{
	unsigned int rest;
	assert(parsed >= buf->data && parsed <=buf->p);
	rest = buf->p - parsed;
	if (rest == 0) {
		buf->p = buf->data;
		buf->space = sizeof(buf->data);
	}
	else {
		memmove(buf->data, parsed, rest);
		buf->p = buf->data + rest;
		buf->space = sizeof(buf->data) - rest;
	}
}


BOOL cmdline_buf_line_complete(struct cmdline_buf *buf)
{
	if (buf->space != 1) {
		return TRUE;
	}
	if (*(buf->p -1) == '\n') {
		return TRUE;
	}
	return FALSE;
}
