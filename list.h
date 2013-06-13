#ifndef _LIST_H_INCLUDE
#define _LIST_H_INCLUDE
#include <stddef.h>

struct mempool;
struct lnode 
{
	void *data;
	struct lnode* next;
};

struct list
{
	struct mempool *pool;
	struct lnode *first;
	struct lnode *last;
};

struct list *l_create(struct mempool *);
/*
void l_release(struct list *);
*/
struct lnode *l_pushback(struct list *, void *data);
struct lnode *l_pushfront(struct list *, void *data);
size_t l_count(struct list *);
/*
struct lnode *l_insert(struct list *, struct lnode*);
*/

#endif
