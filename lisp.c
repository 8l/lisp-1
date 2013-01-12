#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lisp.h"

#define MAXLEN 512

union float_int_conv float_int;

env_t *toplevel;
sexp_t *nil, *t, *dot;

sexp_t *new_sexp(uint8_t type, uint64_t data)
{
	sexp_t *e;
	e = malloc(sizeof(sexp_t));
	e->type = type;
	e->data= data;
	return e;
}

/* Environment */

env_t *new_env(env_t *par)
{
	env_t *env;
	env = malloc(sizeof(env_t));
	env->par = par;
	env->first = NULL;
	return env;
}

sexp_t *env_look_up(env_t *env, sexp_t *sym)	/* TODO: this is slow */
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
		if (strcmp(var, b->var) == 0)
			b->val = val;
	if (env->par)
		env_bind(env->par, var, val);
}

/* String table */

int strtab_len;
int strtab_max;
char **stringtab;

/* TODO: this is slow (and not really necessary anyways) */
char *find_string(const char *s)
{
	int i;
	for (i = 0; i < strtab_len; i++)
		if (strcmp(s, stringtab[i]) == 0)
			return stringtab[i];
	strtab_len++;
	if (strtab_len >= strtab_max) {
		strtab_max *= 2;
		stringtab = realloc(stringtab, strtab_max*sizeof(char*));
	}
	return stringtab[strtab_len-1] = strdup(s);
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
		switch (atm->type) {
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
			print_sexp(car(get_l_code(atm)), out);
			putc(' ', out);
			print_sexp(cdr(get_l_code(atm)), out);
			fprintf(out, ">");
			break;
		case MACRO:
			fprintf(out, "<#Macro ");
			print_sexp(car(get_l_code(atm)), out);
			putc(' ', out);
			print_sexp(cdr(get_l_code(atm)), out);
			fprintf(out, ">");
			break;
		case PRIM:
			fprintf(out, "<#Primitive %p>", get_proc(atm));
			break;
		case SPEC:
			fprintf(out, "<#Specialform %p>", get_proc(atm));
			break;
		}
}

void print_sexp(sexp_t *exp, FILE *out)
{
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
			return symbol(buf);
	}
	if (!havenum)
		return symbol(buf);
	if (type == INT)
		return new_sexp(INT, make_int(i*sign));
	f = sign*(i+f);
	while (exp--)
		f = (expsign > 0) ? f*10.0 : f/10.0;
	return new_sexp(FLOAT, make_float(f));
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
		if (next == dot) {
			fprintf(stderr, "error: 'dot' not allowed at "
					"beginning of list\n");
			return NULL;
		}
		ret = last = cons(next, nil);
		while ((c = nextcharsp(in)) != ')') {
			ungetc(c, in);
			next = read_sexp(in);
			if (next == dot) {
				havedot++;
				continue;
			}
			if (havedot == 0) {
				last->data = make_cons(car(last),
						       cons(next, nil));
				last = cdr(last);
			} else if (havedot == 1) {
				last->data = make_cons(car(last), next);
				havedot++;
			} else {
				fprintf(stderr, "error: only one "
				  "expression after 'dot' allowed\n");
				return NULL;
			}
		}
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
		return cons(symbol("quote"), cons(next, nil));
	}

	if (c == '`') {
		next = read_sexp(in);
		return cons(symbol("backquote"), cons(next, nil));
	}

	if (c == ',') {
		if ((c = nextchar(in)) == '@') {
			next = read_sexp(in);
			return cons(symbol("unquote-splice"), cons(next, nil));
		}
		ungetc(c, in);
		next = read_sexp(in);
		return cons(symbol("unquote"), cons(next, nil));
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

sexp_t *apply_lambda(sexp_t *lambda, sexp_t *args, int ismacro)
{
	env_t *env;
	sexp_t *formals, *code, *ret;

	formals = car(get_l_code(lambda));
	env = new_env(get_l_env(lambda));

	if (list_len(formals) >= 0 && list_len(formals) != list_len(args)) {
		fprintf(stderr, "error: argument count\n");
		return NULL;
	}
	if (iscons(formals)) {
		for (; formals != nil && args != nil;
		       formals = cdr(formals), args = cdr(args)) {
			if (!issym(car(formals))) {
				fprintf(stdout, "error: symbol expected\n");
				return NULL;
			}
			env_bind(env, get_symname(car(formals)), car(args));
			/* If not proper list, bind all remaining args
			 * to the last cdr. */
			if (!islist(cdr(formals))) {
				if (!issym(cdr(formals))) {
					fprintf(stdout, "error: symbol "
					                "expected\n");
					return NULL;
				}
				env_bind(env, get_symname(cdr(formals)),
				         cdr(args));
				break;
			}
		}
	} else if (issym(formals)) {
		env_bind(env, get_symname(formals), args);
	} else if (!isnil(formals)) {
		fprintf(stderr, "error: illegal formal parameters\n");
		return NULL;
	}

	for (code = cdr(get_l_code(lambda)); code != nil; code = cdr(code)) {
		ret = eval(car(code), env);
		if (ismacro)
			ret = eval(ret, env);
	}
	return ret;
}

sexp_t *apply(sexp_t *proc, sexp_t *args, env_t *env)
{
	switch (proc->type) {
	case PRIM:
	case SPEC:
		return (get_proc(proc))(args, env);
	case LAMBDA:
		return apply_lambda(proc, args, 0);
	case MACRO:
		return apply_lambda(proc, args, 1);
	default:
		return NULL;
	}
}

sexp_t *evlis(sexp_t *args, env_t *env)
{
	sexp_t *first;
	if (isnil(args))
		return nil;
	/* force evaluation order */
	first = eval(car(args), env);
	return cons(first, evlis(cdr(args), env));
}

sexp_t *eval(sexp_t *exp, env_t *env)
{
	switch (exp->type) {
	case NIL:
	case INT:
	case FLOAT:
		return exp;
	case SYM:
		return env_look_up(env, exp);
	case CONS: {
		sexp_t *proc;
		if (list_len(exp) < 0) {
			fprintf(stderr, "error: proper list expected\n");
			return NULL;
		}
		proc = eval(car(exp), env);
		if (!proc) {
			fprintf(stderr, "error: not a procedure\n");
			return NULL;
		}
		switch (proc->type) {
		case PRIM:
		case LAMBDA:
			return apply(proc, evlis(cdr(exp), env), env);
		case SPEC:
		case MACRO:
			return apply(proc, cdr(exp), env);
		default:
			return NULL;
		}
	}
	default:
		return NULL;
	}
}

sexp_t *copy_list(sexp_t *l)
{
	if (l == nil)
		return nil;
	return cons(car(l), copy_list(cdr(l)));
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

int init()
{
	if (sizeof(nil->data) != 2*sizeof(void*)) {
		fprintf(stderr, "Error: incompatible with this platform.\n");
		return 1;
	}

	strtab_len = 0;
	strtab_max = 4;
	stringtab = malloc(strtab_max*sizeof(char*));

	nil = new_sexp(NIL,0);
	t = new_sexp(NIL,0);
	dot = new_sexp(NIL,0);

	toplevel = new_env(NULL);

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
	env_bind(toplevel, "label", spec(spec_label));
	env_bind(toplevel, "set", spec(spec_set));
	env_bind(toplevel, "setcar", spec(spec_setcar));
	env_bind(toplevel, "setcdr", spec(spec_setcdr));

	return 0;
}

void dofile(const char *path)
{
	FILE *input;
	sexp_t *e;
	if ((input = fopen(path, "r")) == NULL) {
		fprintf(stderr, "error: could not open %s\n", path);
		return;
	}
	while ((e = read_sexp(input)))
		e = eval(e, toplevel);
	fclose(input);
}

void repl()
{
	sexp_t *e;
	while ((e = read_sexp(stdin))) {
		e = eval(e, toplevel);
		if (e)
			print_sexpnl(e, stdout);
	}
}

void repl_eq()
{
	sexp_t *e1, *e2;
	while ((e1 = read_sexp(stdin)) && (e2 = read_sexp(stdin))) {
		e1 = apply(eval(e1, toplevel), e2, toplevel);
		if (e1)
			print_sexpnl(e1, stdout);
	}
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

	return 0;
}
