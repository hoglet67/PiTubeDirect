/* expr.h expression handling for vasm */
/* (c) in 2002-2015 by Volker Barthelmann and Frank Wille */

#include "hugeint.h"

enum {
  ADD,SUB,MUL,DIV,MOD,NEG,CPL,LAND,LOR,BAND,BOR,XOR,NOT,LSH,RSH,RSHU,
  LT,GT,LEQ,GEQ,NEQ,EQ,NUM,HUG,FLT,SYM
};
#define LAST_EXP_TYPE SYM

struct expr {
  int type;
  struct expr *left;
  struct expr *right;
  union {
    taddr val;
    tfloat flt;
    thuge huge;
    symbol *sym;
  } c;
};

/* Macros for extending the unary operation types (e.g. '<' and '>' for 6502).
   Cpu module has to define EXT_UNARY_EVAL(type,val,res,c) for evaluation. */
#ifndef EXT_UNARY_NAME
#define EXT_UNARY_NAME(s) 0
#endif
#ifndef EXT_UNARY_TYPE
#define EXT_UNARY_TYPE(s) NOT
#endif

/* global variables */
extern char current_pc_char;
extern int unsigned_shift;

/* functions */
expr *new_expr(void);
expr *make_expr(int,expr *,expr *);
expr *copy_tree(expr *);
expr *curpc_expr(void);
expr *parse_expr(char **);
expr *parse_expr_tmplab(char **);
expr *parse_expr_huge(char **);
expr *parse_expr_float(char **);
taddr parse_constexpr(char **);
expr *number_expr(taddr);
expr *huge_expr(thuge);
expr *float_expr(tfloat);
void free_expr(expr *);
int type_of_expr(expr *);
void simplify_expr(expr *);
int eval_expr(expr *,taddr *,section *,taddr);
int eval_expr_huge(expr *,thuge *);
int eval_expr_float(expr *,tfloat *);
void print_expr(FILE *,expr *);
int find_base(expr *,symbol **,section *,taddr);

/* find_base return codes */
#define BASE_ILLEGAL 0
#define BASE_OK 1
#define BASE_PCREL 2
#define BASE_NONE -1  /* no base-symbol assigned, all labels are absolute */
