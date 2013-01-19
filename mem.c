#include <stdlib.h>
#include "lisp.h"

typedef struct gc_mem gc_mem_t;
struct gc_mem {
	void *loc;
	gc_mem_t *next;
};

/* List of allocated objects */
static gc_mem_t *gc_memlist = NULL;
/* Stack of reachable root objects */
static gc_mem_t *gc_root = NULL;

void *gc_alloc(size_t size)
{
	gc_mem_t *new;
	gc_mark();
	gc_sweep();
	new = malloc(sizeof(gc_mem_t));
	new->loc = malloc(size);
	new->next = gc_memlist;
	gc_memlist = new;
	return new->loc;
}

void gc_push(void *obj)
{
	gc_mem_t *new;
	new = malloc(sizeof(gc_mem_t));
	new->loc = obj;
	new->next = gc_root;
	gc_root = new;
}

void gc_pop(void)
{
	gc_mem_t *old;
	old = gc_root;
	gc_root = old->next;
	free(old);
}

void gc_sweep(void)
{
	gc_mem_t **cur, *elt;
	for (cur = &gc_memlist; *cur; ) {
		elt = *cur;
		if (!marked(elt->loc)) {
			*cur = elt->next;
			if (type(elt->loc) == ENV)
				env_clear((void*)elt->loc);
			free(elt->loc);
			free(elt);
		} else {
			((sexp_t*)elt->loc)->type &= ~0x80;
			cur = &elt->next;
		}
	}
}

void mark_recur(sexp_t *exp)
{
	if (marked(exp))
		return;
	exp->type |= 0x80;
	if (type(exp) == ENV) {
		struct binding *b;
		env_t *env = (env_t*)exp;
		for (b = env->first; b; b = b->next)
			mark_recur(b->val);
		if (env->par)
			mark_recur((void*)env->par);
	} else {
		switch (type(exp)) {
		case CONS:
		case LAMBDA:
		case MACRO:
			mark_recur(car(exp));
			mark_recur(cdr(exp));
			break;
		}
	}
}

void gc_mark(void)
{
	gc_mem_t *mem;
	for (mem = gc_root; mem; mem = mem->next)
		if (*(void**)mem->loc)
			mark_recur(*(void**)mem->loc);
}



void gc_dump_stack(void)
{
	gc_mem_t *mem;
	for (mem = gc_root; mem; mem = mem->next)
		print_sexpnl(*(void**)mem->loc, stdout);
}

void gc_dump(void)
{
	gc_mem_t *mem;
	for (mem = gc_memlist; mem; mem = mem->next) {
		if (!marked(mem->loc)) {
			fprintf(stdout, "%%%% ");
		}
		print_sexpnl(mem->loc, stdout);
	}
}

