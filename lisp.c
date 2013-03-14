#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lisp.h"

#define MAXLEN 512

union float_int_conv float_int;

env_t *toplevel;
sexp_t *nil, *t, *dot;

//sexp_t *new_sexp(uint8_t type, uint64_t data)
sexp_t *new_sexp(uint8_t type, DATAT data)
{
	sexp_t *e;
	e = gc_alloc(sizeof(sexp_t));
	e->type = type;
	e->data= data;
	return e;
}

/*
 * Environment
 */

env_t *new_env(env_t *par)
{
	env_t *env;
	env = gc_alloc(sizeof(env_t));
	env->type = ENV;
	env->par = par;
	env->first = NULL;
	return env;
}

void env_clear(env_t *env)
{
	struct binding *b, *next;
	for (b = env->first; b; b = next) {
		free(b->var);
		next = b->next;
		free(b);
	}
}

/* TODO: this is slow */
sexp_t *env_look_up(env_t *env, sexp_t *sym)
{
	struct binding *b;
	for (b = env->first; b; b = b->next)
		if (strcmp(get_symname(sym), b->var) == 0)
			return b->val;
	if (env->par)
		return env_look_up(env->par, sym);
	fprintf(stderr, "error: symbol %s not bound.\n", get_symname(sym));
	return NULL;
}

void env_bind(env_t *env, char *var, sexp_t *val)
{
	struct binding *b = malloc(sizeof(struct binding));
	b->var = strdup(var);
	b->val = val;
	b->next = env->first;
	env->first = b;
}

void env_set(env_t *env, char *var, sexp_t *val)
{
	struct binding *b;
	for (b = env->first; b; b = b->next)
		if (strcmp(var, b->var) == 0) {
			b->val = val;
			return;
		}
	if (env->par)
		env_set(env->par, var, val);
}

/*
 * Symbol list
 */

sexp_t *symlist;

/* TODO: this is slow */
sexp_t *find_symbol(const char *s)
{
	sexp_t *sym;
	sexp_t *tmp;
	for (sym = symlist; sym != nil; sym = cdr(sym))
		if (strcmp(s, get_symname(car(sym))) == 0)
			return car(sym);

	tmp = new_sexp(SYM, make_cons(strdup(s), NULL));
	gc_push(&tmp);
	symlist = cons(tmp, symlist);
	gc_pop();
	return car(symlist);
}

/*
 * Print
 */

void print_atom(sexp_t *atm, FILE *out)
{
	if (atm == nil)
		fprintf(out, "nil");
	else if (atm == t)
		fprintf(out, "t");
	else
		switch (type(atm)) {
		case INT:
			fprintf(out, "%d", get_int(atm));
			break;
		case FLOAT:
			fprintf(out, "%lf", get_float(atm));
			break;
		case SYM:
			fprintf(out, "%s", (char*)car(atm));
			break;
		case LAMBDA:
			fprintf(out, "<#Lambda ");
			print_sexp(proc_params(atm), out);
			putc(' ', out);
			print_sexp(proc_body(atm), out);
			fprintf(out, ">");
			break;
		case MACRO:
			fprintf(out, "<#Macro ");
			print_sexp(proc_params(atm), out);
			putc(' ', out);
			print_sexp(proc_body(atm), out);
			fprintf(out, ">");
			break;
		case PRIM:
			fprintf(out, "<#Primitive %p>", get_prim(atm));
			break;
		case SPEC:
			fprintf(out, "<#Specialform %p>", get_prim(atm));
			break;
		}
}

void print_sexp(sexp_t *exp, FILE *out)
{
	if (type(exp) == ENV)
		fprintf(out, "<#Environment %p>", exp);
	if (isatom(exp)) {
		print_atom(exp, out);
	} else {
		putc('(', out);
		for (; exp != nil && iscons(exp); exp = cdr(exp)) {
			print_sexp(car(exp), out);
			if (!isnil(cdr(exp))) {
				putc(' ', out);
				if (!iscons(cdr(exp))) {
					fprintf(out, ". ");
					print_sexp(cdr(exp), out);
				}
			}
		}
		putc(')', out);
	}
}

/*
 * Read
 */

int nextchar(FILE *in)
{
	int c;
	c = getc(in);
	if (c == ';') {
		while ((c = getc(in)) != '\n');
		c = nextchar(in);
	}
	if (isspace(c))
		c = ' ';
	return c;
}

int nextcharsp(FILE *in)
{
	int c;
	while ((c = nextchar(in)) == ' ');
	return c;
}

/* Reads int, float or symbol */
sexp_t *read_atom(const char *buf)
{
	char c;
	const char *s = buf;
	int i = 0;	// integer part
	double f = 0.0;	// fractional part
	int base = 10;
	double div = base;
	int exp = 0;
	int haveexp = 0;
	int expsign = 1;
	int havenum = 0;
	int sign = 1;
	int type = INT;

	if ((c = *s++) == '-')
		sign = -1;
	else if (c == '+')
		;
	else
		s--;

	while ((c = *s++)) {
		if (isdigit(c) && (type == INT || type == FLOAT)) {
			havenum = 1;
			if (type == INT)	// integer part
				i = i*base + c-'0';
			else {			// fractional part
				f += (c-'0')/div;
				div *= base;
			}
		} else if (c == '.' && type != FLOAT) {
			type = FLOAT;
		} else if (c == 'e' && !haveexp && havenum) {
			havenum = 0;
			haveexp = 1;
			type = FLOAT;
			if ((c = *s++) == '-')
				expsign = -1;
			else if (c == '+')
				;
			else
				s--;
			for (c = *s++; isdigit(c); c = *s++) {
				exp = exp*10 + c-'0';
				havenum = 1;
			}
			s--;
		} else
			return find_symbol(buf);
	}
	if (!havenum)
		return find_symbol(buf);
	if (type == INT)
		return int_(i*sign);
	f = sign*(i+f);
	while (exp--)
		f = (expsign > 0) ? f*10.0 : f/10.0;
	return float_(f);
}

sexp_t *read_sexp(FILE *in)
{
	char buf[MAXLEN], *s = buf;
	int c;
	int havedot = 0;
	sexp_t *last, *next, *ret;

	if ((c = nextcharsp(in)) == EOF)
		return NULL;

	if (c == '(') {
		if ((c = nextcharsp(in)) == ')')
			return nil;
		ungetc(c, in);
		next = read_sexp(in);
		gc_push(&next);
		if (next == dot) {
			fprintf(stderr, "error: 'dot' not allowed at "
					"beginning of list\n");
			gc_pop();
			return NULL;
		}
		ret = last = cons(next, nil);
		gc_pop();
		gc_push(&ret);
		while ((c = nextcharsp(in)) != ')') {
			ungetc(c, in);
			next = read_sexp(in);
			if (next == dot) {
				havedot++;
				continue;
			}
			if (havedot == 0) {
				gc_push(&next);
				last->data = make_cons(car(last),
						       cons(next, nil));
				gc_pop();
				last = cdr(last);
			} else if (havedot == 1) {
				last->data = make_cons(car(last), next);
				havedot++;
			} else {
				fprintf(stderr, "error: only one "
				  "expression after 'dot' allowed\n");
				gc_pop();
				return NULL;
			}
		}
		gc_pop();
		return ret;
	}

	if (c == ')') {
		fprintf(stderr, "error: unexpected \')\'\n");
		return NULL;
	}

	if (c == '.') {
		if ((c = nextchar(in)) == ' ')
			return dot;
		else {
			ungetc(c, in);
			c = '.';
		}
	}

	if (c == '\'') {
		next = read_sexp(in);
		gc_push(&next);
		next = cons(next, nil);
		next = cons(find_symbol("quote"), next);
		gc_pop();
		return next;
	}

	if (c == '`') {
		next = read_sexp(in);
		gc_push(&next);
		next = cons(next, nil);
		next = cons(find_symbol("backquote"), next);
		gc_pop();
		return next;
	}

	if (c == ',') {
		if ((c = nextchar(in)) == '@') {
			next = read_sexp(in);
			gc_push(&next);
			next = cons(next, nil);
			next = cons(find_symbol("unquote-splice"), next);
			gc_pop();
			return next;
		}
		ungetc(c, in);
		next = read_sexp(in);
		gc_push(&next);
		next = cons(next, nil);
		next = cons(find_symbol("unquote"), next);
		gc_pop();
		return next;
	}

	*s++ = c;
	while ((c = nextchar(in)) != EOF &&
	       c != ' ' && c != ')' && c != '(' /*&& c != '\''*/)
		*s++ = c;
	*s = '\0';
	if (c)
		ungetc(c, in);
	return read_atom(buf);
}

/*
 * Eval
 */

env_t *env_extend(env_t *par, sexp_t *params, sexp_t *args)
{
	env_t *env;

	if (list_len(params) >= 0 && list_len(params) != list_len(args)) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}

	env = new_env(par);
	gc_push(&env);
	for (; params != nil; params = cdr(params), args = cdr(args))
		if (iscons(params)) {
			if (!issym(car(params))) {
				fprintf(stdout, "error: symbol expected\n");
				gc_pop();
				return NULL;
			}
			env_bind(env, get_symname(car(params)), car(args));
		} else  {
			if (!issym(params)) {
				fprintf(stdout, "error: symbol expected\n");
				gc_pop();
				return NULL;
			}
			env_bind(env, get_symname(params), args);
			break;
		}
	gc_pop();
	return env;
}

sexp_t *apply(sexp_t *proc, sexp_t *args, env_t *env)
{
	switch (type(proc)) {
	case PRIM:
	case SPEC:
		return (get_prim(proc))(args, env);
	case LAMBDA:
	case MACRO:
		env = env_extend(proc_env(proc), proc_params(proc), args);
		gc_push(&env);
		proc = evblock(proc_body(proc), env, type(proc) == MACRO);
		gc_pop();
		return proc;
	default:
		return NULL;
	}
}

sexp_t *evblock(sexp_t *exp, env_t *env, int ismacro)
{
	sexp_t *ret = NULL;
	gc_push(&ret);
	for (; exp != nil; exp = cdr(exp)) {
		ret = eval(car(exp), env);
		if (ismacro)
			ret = eval(ret, env);
	}
	gc_pop();
	return ret;
}

sexp_t *evlis(sexp_t *args, env_t *env)
{
	sexp_t *e1, *e2;
	if (isnil(args))
		return nil;
	e1 = eval(car(args), env);
	gc_push(&e1);
	e2 = evlis(cdr(args), env);
	gc_push(&e2);
	e1 = cons(e1, e2);
	gc_pop();
	gc_pop();
	return e1;
}

sexp_t *eval(sexp_t *exp, env_t *env)
{
	switch (type(exp)) {
	case NIL:
	case INT:
	case FLOAT:
		return exp;
	case SYM:
		return env_look_up(env, exp);
	case CONS: {
		sexp_t *proc, *args;
		if (list_len(exp) < 0) {
			fprintf(stderr, "error: proper list expected\n");
			return NULL;
		}
		proc = eval(car(exp), env);
		gc_push(&proc);
		args = (type(proc) == PRIM || type(proc) == LAMBDA) ?
			evlis(cdr(exp), env) : cdr(exp);
		gc_push(&args);
		proc = apply(proc, args, env);
		gc_pop();
		gc_pop();
		return proc;
	}
	default:
		return NULL;
	}
}

/*
 * General stuff
 */

sexp_t *copy_list(sexp_t *l)
{
	sexp_t *e;
	if (l == nil)
		return nil;
	e = copy_list(cdr(l));
	gc_push(&e);
	e = cons(car(l), e);
	gc_pop();
	return e;
}

int list_len(sexp_t *e)
{
	int i;
	if (!islist(e))
		return -1;
	for (i = 0; e != nil; e = cdr(e)) {
		if (!islist(cdr(e)))
			return -1;
		i++;
	}
	return i;
}

int init(void)
{
	if (sizeof(nil->data) != 2*sizeof(void*) ||
	    sizeof(PTRT) != sizeof(void*)) {
		fprintf(stderr, "Error: incompatible with this platform.\n");
		return 1;
	}

	nil = new_sexp(NIL,0); gc_push(&nil);
	t = new_sexp(NIL,0); gc_push(&t);
	dot = new_sexp(NIL,0); gc_push(&dot);
	symlist = nil; gc_push(&symlist);
	toplevel = new_env(NULL); gc_push(&toplevel);


	env_bind(toplevel, "nil", nil);
	env_bind(toplevel, "t", t);

	env_bind(toplevel, "atom", prim(prim_atom));
	env_bind(toplevel, "consp", prim(prim_consp));
	env_bind(toplevel, "eq", prim(prim_eq));
	env_bind(toplevel, "cons", prim(prim_cons));
	env_bind(toplevel, "car", prim(prim_car));
	env_bind(toplevel, "cdr", prim(prim_cdr));
	env_bind(toplevel, "list", prim(prim_list));
	env_bind(toplevel, "append", prim(prim_append));
	env_bind(toplevel, "eval", prim(prim_eval));
	env_bind(toplevel, "apply", prim(prim_apply));
	env_bind(toplevel, "progn", prim(prim_progn));
	env_bind(toplevel, "+", prim(prim_add));
	env_bind(toplevel, "-", prim(prim_sub));
	env_bind(toplevel, "*", prim(prim_mul));
	env_bind(toplevel, "/", prim(prim_div));
	env_bind(toplevel, "=", prim(prim_numeq));
	env_bind(toplevel, "<", prim(prim_numlt));
	env_bind(toplevel, ">", prim(prim_numgt));
	env_bind(toplevel, "<=", prim(prim_numle));
	env_bind(toplevel, ">=", prim(prim_numge));
	env_bind(toplevel, "display", prim(prim_display));
	env_bind(toplevel, "newline", prim(prim_newline));
	env_bind(toplevel, "print", prim(prim_print));
	env_bind(toplevel, "read", prim(prim_read));

	env_bind(toplevel, "quote", spec(spec_quote));
	env_bind(toplevel, "backquote", spec(spec_backquote));
	env_bind(toplevel, "cond", spec(spec_cond));
	env_bind(toplevel, "and", spec(spec_and));
	env_bind(toplevel, "or", spec(spec_or));
	env_bind(toplevel, "lambda", spec(spec_lambda));
	env_bind(toplevel, "λ", spec(spec_lambda));
	env_bind(toplevel, "macro", spec(spec_macro));
	env_bind(toplevel, "μ", spec(spec_macro));
	env_bind(toplevel, "label", spec(spec_label));
	env_bind(toplevel, "set", spec(spec_set));
	env_bind(toplevel, "setcar", spec(spec_setcar));
	env_bind(toplevel, "setcdr", spec(spec_setcdr));

	return 0;
}

void clean_up(void)
{
	sexp_t *sym;

	gc_pop();	/* toplevel */
	gc_pop();	/* symlist */
	gc_pop();	/* dot */
	gc_pop();	/* t */
	gc_pop();	/* nil */

	for (sym = symlist; sym != nil; sym = cdr(sym))
		free(get_symname(car(sym)));

	gc_sweep();
}

void dofile(const char *path)
{
	FILE *input;
	sexp_t *e = NULL;
	if ((input = fopen(path, "r")) == NULL) {
		fprintf(stderr, "error: could not open %s\n", path);
		return;
	}
	gc_push(&e);
	while ((e = read_sexp(input))) {
		e = eval(e, toplevel);
		e = NULL;
	}
	gc_pop();
	fclose(input);
}

/* normal repl */
void repl()
{
	sexp_t *e = NULL;
	gc_push(&e);
	while ((e = read_sexp(stdin))) {
		e = eval(e, toplevel);
		if (e)
			print_sexpnl(e, stdout);
		e = NULL;
	}
	gc_pop();
}

/* evalquote repl */
void repl_eq()
{
	sexp_t *e1 = NULL, *e2 = NULL;
	gc_push(&e1);
	gc_push(&e2);
	while ((e1 = read_sexp(stdin)) && (e2 = read_sexp(stdin))) {
		e1 = apply(eval(e1, toplevel), e2, toplevel);
		if (e1)
			print_sexpnl(e1, stdout);
		e1 = e2 = NULL;
	}
	gc_pop();
	gc_pop();
}

int main(int argc, char *argv[])
{
	if (init())
		return 1;

	dofile("lib.lsp");

	if (argc > 1 && strcmp(argv[1], "-eq") == 0)
		repl_eq();
	else
		repl();
	clean_up();

	return 0;
}
