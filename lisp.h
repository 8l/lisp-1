#ifndef LISP_H
#define LISP_H

#include <stdint.h>
#include <stdio.h>

#define NIL	0
#define INT	1
#define FLOAT	2
#define SYM	3
#define CONS	4
#define LAMBDA	5
#define MACRO	6
#define PRIM	7
#define SPEC	8

typedef struct sexp sexp_t;
struct sexp {
	uint8_t type;
	uint64_t data;
};

typedef struct env env_t;
struct env {
	env_t *par;
	struct binding {
		char *var;
		sexp_t *val;
		struct binding *next;
	} *first;
};

union float_int_conv {
	double f;
	uint64_t i;
};
extern union float_int_conv float_int;
extern sexp_t *nil, *t, *dot;

sexp_t *copy_list(sexp_t *l);
int list_len(sexp_t *e);

env_t *toplevel;
sexp_t *new_sexp(uint8_t type, uint64_t data);

env_t *new_env(env_t *par);
sexp_t *env_look_up(env_t *env, sexp_t *sym);
void env_bind(env_t *env, char *var, sexp_t *val);
void env_set(env_t *env, char *var, sexp_t *val);

char *find_string(const char *s);

void print_sexp(sexp_t *exp, FILE *out);
#define print_sexpnl(exp, out)\
	(print_sexp(exp,out), putc('\n',out))
sexp_t *read_sexp(FILE *in);

sexp_t *apply(sexp_t *proc, sexp_t *args, env_t *env);
sexp_t *evlis(sexp_t *args, env_t *env);
sexp_t *eval(sexp_t *exp, env_t *env);

/*
 * Primitive functions
 * and Special forms
 */
sexp_t *prim_atom(sexp_t *args);
sexp_t *prim_consp(sexp_t *args);
sexp_t *prim_eq(sexp_t *args);
sexp_t *prim_cons(sexp_t *args);
sexp_t *prim_car(sexp_t *args);
sexp_t *prim_cdr(sexp_t *args);
sexp_t *prim_list(sexp_t *args);
sexp_t *prim_append(sexp_t *args);
sexp_t *prim_eval(sexp_t *args, env_t *env);
sexp_t *prim_apply(sexp_t *args, env_t *env);
sexp_t *prim_progn(sexp_t *args);
sexp_t *prim_add(sexp_t *args);
sexp_t *prim_sub(sexp_t *args);
sexp_t *prim_mul(sexp_t *args);
sexp_t *prim_div(sexp_t *args);
sexp_t *prim_numeq(sexp_t *args);
sexp_t *prim_numlt(sexp_t *args);
sexp_t *prim_numgt(sexp_t *args);
sexp_t *prim_numle(sexp_t *args);
sexp_t *prim_numge(sexp_t *args);
sexp_t *prim_display(sexp_t *args);
sexp_t *prim_newline();
sexp_t *prim_print(sexp_t *args);
sexp_t *prim_read();

sexp_t *spec_quote(sexp_t *args);
sexp_t *spec_backquote(sexp_t *args, env_t *env);
sexp_t *spec_cond(sexp_t *args, env_t *env);
sexp_t *spec_and(sexp_t *args, env_t *env);
sexp_t *spec_or(sexp_t *args, env_t *env);
sexp_t *spec_lambda(sexp_t *args, env_t *env);
sexp_t *spec_macro(sexp_t *args, env_t *env);
sexp_t *spec_label(sexp_t *args, env_t *env);
sexp_t *spec_set(sexp_t *args, env_t *env);
sexp_t *spec_setcar(sexp_t *args, env_t *env);
sexp_t *spec_setcdr(sexp_t *args, env_t *env);

#define isint(X)	((X)->type == INT)
#define isfloat(X)	((X)->type == FLOAT)
#define isnum(X)	(isint(X) || isfloat(X))
#define issym(X)	((X)->type == SYM)
#define islambda(X)	((X)->type == LAMBDA)
#define isprim(X)	((X)->type == PRIM)
#define isspec(X)	((X)->type == SPEC)
#define isatom(X)	((X)->type != CONS)
#define iscons(X)	((X)->type == CONS)
#define isnil(X)	((X) == nil)
#define islist(X)	(iscons(X) || isnil(X))

#define make_cons(a, b)	(((uint64_t) ((uint32_t)(b)) << sizeof(void*)*8) |\
				(uint32_t)(a))
#define cons(a, b)	(new_sexp(CONS, make_cons((a), (b))))

#define get_car(b)	((void*)((uint32_t)(b)))
#define get_cdr(a)	((void*)((uint32_t)((a) >> sizeof(void*)*8)))
#define car(a)		((sexp_t*)(get_car((a)->data)))
#define cdr(a)		((sexp_t*)(get_cdr((a)->data)))

#define symbol(s)	(new_sexp(SYM, make_cons(find_string((s)), NULL)))
#define get_symname(s)	((char*)car(s))

#define make_int(a)	((uint64_t) (a))
#define get_int(a)	((int32_t) (a)->data)
#define int_(a)		(new_sexp(INT, make_int(a)))

#define make_float(a)	(float_int.f = (a), float_int.i)
#define get_float(a)	(float_int.i = (a)->data, (double)float_int.f)
#define float_(a)	(new_sexp(FLOAT, make_float(a)))

#define prim(a)		(new_sexp(PRIM, make_cons((a), NULL)))
#define spec(a)		(new_sexp(SPEC, make_cons((a), NULL)))
#define lambda(a,b)	(new_sexp(LAMBDA, make_cons((a), (b))))
#define macro(a,b)	(new_sexp(MACRO, make_cons((a), (b))))
#define get_l_code(a)	(car(a))
#define get_l_env(a)	((env_t*)cdr(a))
#define get_proc(a)	((sexp_t *(*)())car(a))

#endif
