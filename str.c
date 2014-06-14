#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "mempool.h"
#include "str.h"


struct str *s_new(struct mempool *pool)
{
	struct str *str;
	str = p_alloc(pool, sizeof(struct str));
	str->len = 0;
	str->data = NULL;
	return str;
}


struct str *dup_cstr(struct mempool *pool, const char *c_str)
{
	struct str *str;
	char *data;
	int len;
	str = p_alloc(pool, sizeof(struct str));
	len = strlen(c_str);
	data = p_alloc(pool, len);
	memcpy(data, c_str, len);
	str->len = len;
	str->data = data;
	return str;
}


struct str *s_make(struct mempool *pool, const char *data, size_t len)
{
	struct str *str;
	str = p_alloc(pool, sizeof(struct str));
	str->len = len;
	str->data = data;
	return str;
}


BOOL s_empty(struct str *str)
{
	if (str != NULL && str->data != NULL && str->len != 0) {
		return 0;
	}
	return 1;
}


int s_isnumeric(struct str *str)
{
	int i;
	const char *p;
	p = str->data;
	for (i=0; i< str->len; ++i) {
		if (!isdigit(p[i])) {
			return 0;
		}
	}
	return 1;
}


int s_atoi(struct str *str)
{
	int result;
	int isnegative; 
	int i;
	const char *p;
	result = 0;
	p = str->data;
	i = 0;
	if (p[i] == '-') {
		isnegative  = 1;
		++i;
	}
	else {
		isnegative = 0;
	}
	for (; i< str->len; ++i) {
		if (p[i] >= '0' && p[i] <= '9') {
			result *= 10;
			result += p[i] - '0';
		}
		else {
			break;
		}
	}
	if (isnegative) {
		return result * -1;
	}
	return result;
}


int cstrcmp(struct str *l, struct str *r)
{
	int minlength;
	int result;
	if (l->len > r->len) {
		minlength = r->len;
	}
	else {
		minlength = l->len;
	}
	result = strncmp(l->data, r->data, minlength);
	if (result != 0) {
		return result;
	}
	return l->len - r->len;
}


char *p_sdup(struct mempool *pool, struct str *str)
{
	char *p;
	p = p_alloc(pool, str->len+1);
	memcpy(p, str->data, str->len+1);
	p[str->len] = '\0';
	return p;
}


int s_print(struct str *str, void *_stream)
{
	int i;
	const char *p;
	FILE *stream;
	stream = _stream;
	p = str->data;
	for (i=0; i< str->len; ++i) {
		fputc(p[i], stream);
	}
	return str->len;
}


