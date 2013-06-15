#ifndef _STR_H_INCLUDE
#define _STR_H_INCLUDE

struct mempool;

struct cstr {
	size_t len;
	const char *data;
};

/* create a new empty cstr */
struct cstr *new_cstr(struct mempool *);
/* create a new cstr and duplicate contents from a c style string */
struct cstr *dup_cstr(struct mempool *, const char *);
/* create a new cstr and use specified data as it's contents */
struct cstr *make_cstr(struct mempool *, const char *, size_t);

int cstr_empty(struct cstr *);
int cstrcmp(struct cstr *l, struct cstr *);
char *p_cstrdup(struct mempool *, struct cstr *);
int cstr_isnumeric(struct cstr *);
int cstr_atoi(struct cstr *);
int cstr_print(struct cstr *, void*); /* the 2nd param should be FILE*, and i dong't know how to declare it without include stdio.h */
	
#endif
