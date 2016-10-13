/* syntax.h  syntax header file for vasm */
/* (c) in 2002,2012,2014 by Frank Wille */

/* macros to recognize identifiers */
#define ISIDSTART(x) ((x)=='.'||(x)=='_'||isalpha((unsigned char)(x)))
#define ISIDCHAR(x) ((x)=='_'||isalnum((unsigned char)(x)))
#define ISEOL(x) (*(x)=='\0' || *(x)==commentchar)

/* result of a boolean operation */
#define BOOLEAN(x) -(x)

/* operator separation characters */
#ifndef OPERSEP_COMMA
#define OPERSEP_COMMA 1
#endif
#ifndef OPERSEP_BLANK
#define OPERSEP_BLANK 0
#endif

/* overwrite macro defaults */
#define MAXMACPARAMS 35
char *my_skip_macro_arg(char *);
#define SKIP_MACRO_ARGNAME(p) my_skip_macro_arg(p)
