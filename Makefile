lisp_64: lisp.c prim.c mem.c lisp.h
	cc -g -Wall -Wextra -DBIT64 lisp.c prim.c mem.c -o lisp
lisp_32: lisp.c prim.c mem.c lisp.h
	cc -g -Wall -Wextra lisp.c prim.c mem.c -o lisp
