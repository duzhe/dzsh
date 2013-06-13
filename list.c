#include "list.h"
#include "mempool.h"
#include <stdlib.h>


struct list *l_create(struct mempool *pool)
{
	struct list *l = p_alloc(pool, sizeof(struct list));
	l->pool = pool;
	l->first = NULL;
	l->last = NULL;
	return l;
}


struct lnode *l_pushback(struct list *l, void *data)
{
	struct lnode *node;
	node = p_alloc(l->pool, sizeof(struct lnode));
	node->next = NULL;
	if (l->last == NULL) {
		l->first = node;
	}
	else {
		l->last->next = node;
	}
	l->last = node;
	node->data = data;
	return node;
}


struct lnode *l_pushfront(struct list *l, void *data)
{
	struct lnode *node;
	node = p_alloc(l->pool, sizeof(struct lnode));
	node->next = l->first;
	l->first = node;
	if (l->last == NULL) {
		l->last = node;
	}
	node->data = data;
	return node;
}

/*
struct lnode *l_insert(struct list *, struct lnode*, void *);
*/
