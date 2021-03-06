#ifndef _MEMPOOL_H_INCLUDE
#define _MEMPOOL_H_INCLUDE
#include <stddef.h>
#include "bool.h"

struct list;
struct mempage
{
	char *buf; /* char * type for easy caculate*/
	size_t pagesize;
	int used;
	struct mempage *next;
};

struct mempool
{
	size_t pagesize;
	struct mempage *first;
	struct mempage *now;
	struct list *large;
	/*
	size_t nodesize;
	*/
};

struct mempool *p_create(size_t pagesize);
void p_clear(struct mempool *);
void p_destroy(struct mempool*);
void *p_alloc(struct mempool *, size_t);
void *p_large_alloc(struct mempool*, size_t);
char *p_strdup(struct mempool *, const char *);
char *p_strndup(struct mempool *, const char *, size_t);
void *p_dup(struct mempool *, void *, size_t);


#endif
