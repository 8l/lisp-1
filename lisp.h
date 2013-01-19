#ifndef LISP_H
#define LISP_H

#include <stdint.h>
#include <stdio.h>

#define NIL	0x0
#define INT	0x1
#define FLOAT	0x2
#define SYM	0x3
#define CONS	0x4
#define LAMBDA	0x5
#define MACRO	0x6
#define PRIM	0x7
#define SPEC	0x8
#define ENV	0x9

typedef struct sexp sexp_t;
struct sexp {
	uint8_t type;
	uint64_t data;
};

typedef struct env env_t;
struct env {
	uint8_t type;
	env_t *par;
	struct binding {
		char *var;
		sexp_t *val;
		struct binding *next;
	} *first;
};

union float_int_conv { double f; uint64_t i; };
extern union float_int_conv float_int;
extern sexp_t *nil, *t, *dot;

env_t *toplevel;

void    gc_dump(void);
void    gc_dump_stack(void);
void   *gc_alloc(size_t size);
void    gc_push(void *obj);
void    gc_pop(void);
void    gc_mark(void);
void    gc_sweep(void);

sexp_t *copy_list(sexp_t *l);
int     list_len(sexp_t *e);

sexp_t *new_sexp(uint8_t type, uint64_t data);

env_t  *new_env(env_t *par);
env_t  *env_extend(env_t *par, sexp_t *params, sexp_t *args);
void    env_clear(env_t *env);
sexp_t *env_look_up(env_t *env, sexp_t *sym);
void    env_bind(env_t *env, char *var, sexp_t *val);
void    env_set(env_t *env, char *var, sexp_t *val);

sexp_t *find_symbol(const char *s);

void    print_sexp(sexp_t *exp, FILE *out);
#define print_sexpnl(exp, out)\
	(print_sexp(exp,out), putc('\n',out))
sexp_t *read_sexp(FILE *in);

sexp_t *apply(sexp_t *proc, sexp_t *args, env_t *env);
sexp_t *evlis(sexp_t *args, env_t *env);
sexp_t *evblock(sexp_t *exp, env_t *env, int ismacro);
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

#define type(X)		(((sexp_t*)(X))->type & 0x7F)
#define marked(X)	(((sexp_t*)(X))->type & 0x80)
#define isint(X)	(type(X) == INT)
#define isfloat(X)	(type(X) == FLOAT)
#define isnum(X)	(isint(X) || isfloat(X))
#define issym(X)	(type(X) == SYM)
#define islambda(X)	(type(X) == LAMBDA)
#define isprim(X)	(type(X) == PRIM)
#define isspec(X)	(type(X) == SPEC)
#define isatom(X)	(type(X) != CONS)
#define iscons(X)	(type(X) == CONS)
#define isnil(X)	((X) == nil)
#define islist(X)	(iscons(X) || isnil(X))

#define make_cons(a, b)	(((uint64_t) ((uint32_t)(b)) << sizeof(void*)*8) |\
				(uint32_t)(a))
#define cons(a, b)	(new_sexp(CONS, make_cons((a), (b))))

#define get_car(b)	((void*)((uint32_t)(b)))
#define get_cdr(a)	((void*)((uint32_t)((a) >> sizeof(void*)*8)))
#define car(a)		((sexp_t*)(get_car((a)->data)))
#define cdr(a)		((sexp_t*)(get_cdr((a)->data)))

#define get_symname(s)	((char*)car(s))

#define make_int(a)	((uint64_t) (a))
#define get_int(a)	((int32_t) (a)->data)
#define int_(a)		(new_sexp(INT, make_int(a)))

#define make_float(a)	(float_int.f = (a), float_int.i)
#define get_float(a)	(float_int.i = (a)->data, (double)float_int.f)
#define float_(a)	(new_sexp(FLOAT, make_float(a)))

#define prim(a)		(new_sexp(PRIM, make_cons((a), NULL)))
#define spec(a)		(new_sexp(SPEC, make_cons((a), NULL)))
#define lambda(a,env)	(new_sexp(LAMBDA, make_cons((a), (env))))
#define macro(a,env)	(new_sexp(MACRO, make_cons((a), (env))))
#define proc_params(a)	(car(car(a)))
#define proc_body(a)	(cdr(car(a)))
#define proc_env(a)	((env_t*)cdr(a))
#define get_prim(a)	((sexp_t *(*)())car(a))

#endif
