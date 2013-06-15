#include "list.h"
#include "mempool.h"
#include <stdlib.h>
#include <string.h>


struct mempool *p_create(size_t pagesize)
{
	char *pbuf;
	struct mempage *page;
	struct mempool *pool;
	if (pagesize <= sizeof(struct mempool) + sizeof(struct mempage)) {
		return NULL;
	}
	pbuf = malloc(pagesize);
	pool = (struct mempool *)pbuf;
	page = (struct mempage *)(pbuf + sizeof(struct mempool));
	pool->pagesize = pagesize;
	pool->first = page;
	pool->now = page;
	page->buf = pbuf;
	page->pagesize = pagesize;
	page->used = sizeof(struct mempool) + sizeof(struct mempage);
	page->next = NULL;
	pool->large = NULL;
	return pool;
}


void p_clear(struct mempool *pool)
{
	struct lnode *large;
	struct mempage *page;

	if (pool->large != NULL) {
		large = pool->large->first;
		while (large != NULL) {
			free(large->data);
			large = large->next;
		}
	}

	page = pool->first;
	page->used = sizeof(struct mempool) + sizeof(struct mempage);
	page = page->next;
	while (page != NULL) {
		page->used = sizeof(struct mempage);
		page = page->next;
	}
	pool->now = pool->first;
}


void p_destroy(struct mempool *pool)
{
	struct lnode *large;
	struct mempage *page;
	struct mempage *pagenext;

	if (pool->large != NULL) {
		large = pool->large->first;
		while (large != NULL) {
			free(large->data);
			large = large->next;
		}
	}

	page = pool->first;
	while (page != NULL) {
		pagenext = page->next;
		free(page->buf);
		page = pagenext;
	}
}


void *p_alloc(struct mempool *pool, size_t size)
{
	struct mempage *page; 
	void *r;
	if (size >= pool->pagesize - sizeof(struct mempage)) {
		return p_large_alloc(pool, size);
	}
	page = pool->now;
	if (page->pagesize - page->used < size) {
		if (page->next == NULL) {
			page = malloc(pool->pagesize);
			page->buf = (void*)page;
			page->pagesize = pool->pagesize;
			page->used = sizeof(struct mempool);
			page->next = NULL;
			pool->now->next = page;
			pool->now = page;
		}
		else {
			page = page->next;
			pool->now = page;
		}
	}
	r = page->buf + page->used;
	page->used += size;
	return r;
}


void *p_large_alloc(struct mempool* pool, size_t size)
{
	void *p;
	if (pool->large == NULL) {
		pool->large = l_create(pool);
	}
	p = malloc(size);
	l_pushback(pool->large, p);
	return p;
}


char *p_strdup(struct mempool *pool, const char *str)
{
	size_t len;
	char *new;
	len = strlen(str);
	new = p_alloc(pool, len +1);
	memcpy(new, str, len + 1);
	return new;
}


char *p_strndup(struct mempool *pool, const char *str, size_t len)
{
	char *new;
	new = p_alloc(pool, len +1);
	memcpy(new, str, len);
	new[len] = '\0';
	return new;
}
