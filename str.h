#ifndef _STR_H_INCLUDE
#define _STR_H_INCLUDE

#include "bool.h"

struct mempool;

struct str {
	size_t len;
	const char *data;
};

/* create a new empty str */
struct str *s_new(struct mempool *);
/* create a new str and duplicate contents from a c style string */
struct str *s_dup(struct mempool *, const char *);
/* create a new str and use specified data as it's contents */
struct str *s_make(struct mempool *, const char *, size_t);

BOOL s_empty(struct str *);
int s_cmp(struct str *l, struct str *);
char *p_sdup(struct mempool *, struct str *);
int s_isnumeric(struct str *);
int s_atoi(struct str *);
int s_print(struct str *, void*); /* the 2nd param should be FILE*, and i dong't know how to declare it without include stdio.h */
	
#endif
