#include <string.h>
#include "lisp.h"

/*
 * Primitive procedures
 */

sexp_t *prim_atom(sexp_t *args)
{
	if (list_len(args) != 1) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	if (isatom(car(args)))
		return t;
	return nil;
}

sexp_t *prim_consp(sexp_t *args)
{
	if (list_len(args) != 1) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	if (iscons(car(args)))
		return t;
	return nil;
}

sexp_t *prim_eq(sexp_t *args)
{
	if (list_len(args) < 2)
		return t;
	if (type(car(args)) == NIL || type(car(cdr(args))) == NIL)
		return (car(args) == car(cdr(args))) ? t : nil;
	if ((car(args)->data == car(cdr(args))->data) &&
	    (prim_eq(cdr(args)) == t))
		return t;
	return nil;
}

sexp_t *prim_cons(sexp_t *args)
{
	if (list_len(args) != 2) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	return cons(car(args), car(cdr(args)));
}

sexp_t *prim_car(sexp_t *args)
{
	if (list_len(args) != 1) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	if (!iscons(car(args))) {
		fprintf(stderr, "error: cons expected\n");
		return NULL;
	}
	return car(car(args));
}

sexp_t *prim_cdr(sexp_t *args)
{
	if (list_len(args) != 1) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	if (!iscons(car(args))) {
		fprintf(stderr, "error: cons expected\n");
		return NULL;
	}
	return cdr(car(args));
}

sexp_t *prim_list(sexp_t *args)
{
	return args;
}

sexp_t *prim_append(sexp_t *args)
{
	sexp_t *lst, *ret;
	if (list_len(args) == 0)
		return nil;
	if (isnil(car(args)))
		return prim_append(cdr(args));
	if (isnil(cdr(args)))
		return copy_list(car(args));
	if (!iscons(car(args)) || list_len(car(args)) < 0) {
		fprintf(stderr, "error: proper list expected\n");
		return NULL;
	}
	for (ret = lst = copy_list(car(args)); cdr(lst) != nil; lst = cdr(lst))
		;
	gc_push(&ret);
	lst->data = make_cons(car(lst), prim_append(cdr(args)));
	gc_pop();
	return ret;
}

sexp_t *prim_eval(sexp_t *args, env_t *env)
{
	if (list_len(args) != 1) {
		fprintf(stderr, "error: argument count eval\n");
		return NULL;
	}
	return eval(car(args), env);
}

sexp_t *prim_apply(sexp_t *args, env_t *env)
{
	if (list_len(args) != 2) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	return apply(car(args), car(cdr(args)), env);
}

sexp_t *prim_progn(sexp_t *args)
{
	for (; cdr(args) != nil; args = cdr(args));
	return car(args);
}

#define addmul(F,OP,ID) \
	sexp_t *n1, *n2; \
	if (list_len(args) == 0) \
		return int_(ID); \
	n1 = car(args); \
	if (!isnum(n1)) { \
		fprintf(stderr, "error: number expected\n"); \
		return NULL; \
	} \
	if (list_len(args) == 1) \
		return new_sexp(type(n1), n1->data); \
	n2 = F(cdr(args)); \
	if (!n2) \
		return NULL; \
	gc_push(&n2); \
	if (isint(n1)) { \
		n2 = isint(n2) ? \
			int_(get_int(n1) OP get_int(n2)) : \
			float_(get_int(n1) OP get_float(n2)); \
		gc_pop(); \
		return n2; \
	} else { \
		/* this is necessary because of sequence points */ \
		double f = get_float(n2); \
		n2 = isint(n2) ?  \
			float_(get_float(n1) OP get_int(n2)) : \
			float_(get_float(n1) OP f); \
		gc_pop(); \
		return n2; \
	}
sexp_t *prim_add(sexp_t *args) { addmul(prim_add, +, 0); }
sexp_t *prim_mul(sexp_t *args) { addmul(prim_mul, *, 1); }
#undef addmul

sexp_t *prim_sub(sexp_t *args)
{
	sexp_t *n1, *n2;
	if (list_len(args) == 0) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	n1 = car(args);
	if (!isnum(n1)) {
		fprintf(stderr, "error: number expected\n");
		return NULL;
	}
	if (list_len(args) == 1)
		return isint(n1) ? int_(-get_int(n1)) : float_(-get_float(n1));
	n2 = prim_add(cdr(args));
	if (!n2)
		return NULL;
	gc_push(&n2);
	n2->data = isint(n2) ?
		make_int(-get_int(n2)) :
		make_float(-get_float(n2));
	n2 = cons(n2, nil);
	n2 = cons(n1, n2);
	n2 = prim_add(n2);
	gc_pop();
	return n2;
}

/* TODO; return ints when possible */
sexp_t *prim_div(sexp_t *args)
{
	sexp_t *n1, *n2;
	if (list_len(args) == 0) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	n1 = car(args);
	if (!isnum(n1)) {
		fprintf(stderr, "error: number expected\n");
		return NULL;
	}
	if (list_len(args) == 1)
		return isint(n1) ? float_(1.0/get_int(n1)) :
			float_(1.0/get_float(n1));
	n2 = prim_mul(cdr(args));
	if (!n2)
		return NULL;
	gc_push(&n2);
	n2->data = isint(n2) ?
		make_float(1.0/get_int(n2)) :
		make_float(1.0/get_float(n2));
	n2->type = FLOAT;
	n2 = cons(n2, nil);
	n2 = cons(n1, n2);
	n2 = prim_mul(n2);
	gc_pop();
	return n2;
}

#define num_cmp(F,OP) \
	sexp_t *n1, *n2;\
	int test;\
	if (list_len(args) == 0)\
		return t;\
	n1 = car(args);\
	if (!isnum(n1)) {\
		fprintf(stderr, "error: number expected\n");\
		return NULL;\
	}\
	if (list_len(args) < 2)\
		return t;\
	n2 = car(cdr(args));\
	if (!isnum(n2)) {\
		fprintf(stderr, "error: number expected\n");\
		return NULL;\
	}\
	if (isint(n1))\
		test = isint(n2) ?\
			(get_int(n1) OP get_int(n2)) :\
			(get_int(n1) OP get_float(n2));\
	else {\
		/* this is necessary because of to sequence points */\
		double f = get_float(n2);\
		test = isint(n2) ?\
			(get_float(n1) OP get_int(n2)) :\
			(get_float(n1) OP f);\
	}\
	if (!test)\
		return nil;\
	return F(cdr(args));
sexp_t *prim_numeq(sexp_t *args) { num_cmp(prim_numeq, ==); }
sexp_t *prim_numlt(sexp_t *args) { num_cmp(prim_numlt, <); }
sexp_t *prim_numgt(sexp_t *args) { num_cmp(prim_numgt, >); }
sexp_t *prim_numle(sexp_t *args) { num_cmp(prim_numle, <=); }
sexp_t *prim_numge(sexp_t *args) { num_cmp(prim_numge, >=); }
#undef num_cmp

sexp_t *prim_display(sexp_t *args)
{
	for (; args != nil; args = cdr(args)) {
		print_sexp(car(args), stdout);
		putc(' ', stdout);
	}
	return NULL;
}

sexp_t *prim_newline()
{
	putc('\n', stdout);
	return NULL;
}

sexp_t *prim_print(sexp_t *args)
{
	prim_display(args);
	prim_newline();
	return NULL;
}

sexp_t *prim_read()
{
	return read_sexp(stdin);
}

/*
 * Special forms
 */

sexp_t *spec_quote(sexp_t *args)
{
	return car(args);
}

sexp_t *spec_backquote(sexp_t *arg, env_t *env)
{
	sexp_t *list, *prev;
	arg = car(arg);
	if (isatom(arg))
		return arg;
	/* what a mess */
	for (prev = nil, list = arg; list != nil; prev = list, list = cdr(list))
		if (iscons(car(list)) && issym(car(car(list)))) {
			if (strcmp(get_symname(car(car(list))), "unquote") == 0)
				list->data = make_cons(
					eval(car(cdr(car(list))), env),
					          cdr(list));
			else if (strcmp(get_symname(car(car(list))),
			         "unquote-splice") == 0) {
				sexp_t *next = cdr(list);
				sexp_t *new = eval(car(cdr(car(list))), env);
				if (prev == nil) {
					gc_push(&next);
					next = cons(next, nil);
					next = cons(new, next);
					next = prim_append(next);
					gc_pop();
					return next;
				} else {
					gc_push(&next);
					next = cons(next, nil);
					next = cons(new, next);
					prev->data = make_cons(car(prev), next);
					gc_pop();
				}
			}
		}
	return arg;
}

sexp_t *spec_cond(sexp_t *args, env_t *env)
{
	if (args == nil)
		return NULL;
	if (eval(car(car(args)), env) != nil)
		return eval(car(cdr(car(args))), env);
	return spec_cond(cdr(args), env);
}

sexp_t *spec_and(sexp_t *args, env_t *env)
{
	if (list_len(args) == 0)
		return t;
	if (list_len(args) == 1)
		return eval(car(args), env);
	if (eval(car(args), env) == nil)
		return nil;
	return spec_and(cdr(args), env);
}

sexp_t *spec_or(sexp_t *args, env_t *env)
{
	sexp_t *ret;
	if (list_len(args) == 0)
		return nil;
	if (list_len(args) == 1)
		return eval(car(args), env);
	if ((ret = eval(car(args), env)) != nil)
		return ret;
	return spec_or(cdr(args), env);
}

sexp_t *spec_lambda(sexp_t *args, env_t *env)
{
	if (list_len(args) < 2) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	return lambda(args, env);
}

sexp_t *spec_macro(sexp_t *args, env_t *env)
{
	if (list_len(args) < 2) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	return macro(args, env);
}

sexp_t *spec_label(sexp_t *args, env_t *env)
{
	if (list_len(args) < 2) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	if (!issym(car(args))) {
		fprintf(stderr, "error: symbol expected\n");
		return NULL;
	}
	env_bind(toplevel, get_symname(car(args)),
	         eval(car(cdr(args)), env));
	return NULL;
}

sexp_t *spec_set(sexp_t *args, env_t *env)
{
	if (list_len(args) < 2) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	if (!issym(car(args))) {
		fprintf(stderr, "error: symbol expected\n");
		return NULL;
	}
	env_set(env, get_symname(car(args)), eval(car(cdr(args)), env));
	return NULL;
}

sexp_t *spec_setcar(sexp_t *args, env_t *env)
{
	sexp_t *cs;
	if (list_len(args) < 2) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	cs = eval(car(args), env);
	if (!iscons(cs)) {
		fprintf(stderr, "error: cons expected\n");
		return NULL;
	}
	gc_push(&cs);
	cs->data = make_cons(eval(car(cdr(args)), env), cdr(cs));
	gc_pop();
	return NULL;
}

sexp_t *spec_setcdr(sexp_t *args, env_t *env)
{
	sexp_t *cs;
	if (list_len(args) < 2) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	cs = eval(car(args), env);
	if (!iscons(cs)) {
		fprintf(stderr, "error: cons expected\n");
		return NULL;
	}
	gc_push(&cs);
	cs->data = make_cons(car(cs), eval(car(cdr(args)), env));
	gc_pop();
	return NULL;
}
