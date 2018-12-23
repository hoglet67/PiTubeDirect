/* vasm.h  main header file for vasm */
/* (c) in 2002-2017 by Volker Barthelmann */

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

typedef struct symbol symbol;
typedef struct section section;
typedef struct dblock dblock;
typedef struct sblock sblock;
typedef struct expr expr;
typedef struct macro macro;
typedef struct source source;
typedef struct listing listing;
typedef struct regsym regsym;

#define MAXPADBYTES 8  /* max. pattern size to pad alignments */

#include "cpu.h"
#include "symbol.h"
#include "reloc.h"
#include "syntax.h"
#include "symtab.h"
#include "expr.h"
#include "parse.h"
#include "atom.h"
#include "error.h"
#include "cond.h"
#include "supp.h"

#if defined(BIGENDIAN)&&!defined(LITTLEENDIAN)
#define LITTLEENDIAN (!BIGENDIAN)
#endif

#if !defined(BIGENDIAN)&&defined(LITTLEENDIAN)
#define BIGENDIAN (!LITTLEENDIAN)
#endif

#ifndef MNEMONIC_VALID
#define MNEMONIC_VALID(i) 1
#endif

#ifndef OPERAND_OPTIONAL
#define OPERAND_OPTIONAL(p,t) 0
#endif

#ifndef START_PARENTH
#define START_PARENTH(x) ((x)=='(')
#endif

#ifndef END_PARENTH
#define END_PARENTH(x) ((x)==')')
#endif

#ifndef CHKIDEND
#define CHKIDEND(s,e) (e)
#endif

#define MAXPATHLEN 1024

/* include paths */
struct include_path {
  struct include_path *next;
  char *path;
};

/* source texts (main file, include files or macros) */
struct source {
  struct source *parent;
  int parent_line;
  char *name;
  char *text;
  size_t size;
  macro *macro;
  unsigned long repeat;
  char *irpname;
  struct macarg *irpvals;
  int cond_level;
  struct macarg *argnames;
  int num_params;
  char *param[MAXMACPARAMS+1];
  int param_len[MAXMACPARAMS+1];
#if MAX_QUALIFIERS > 0
  int num_quals;
  char *qual[MAX_QUALIFIERS];
  int qual_len[MAX_QUALIFIERS];
#endif
  unsigned long id;
  char *srcptr;
  int line;
  size_t bufsize;
  char *linebuf;
#ifdef CARGSYM
  expr *cargexp;
#endif
#ifdef REPTNSYM
  long reptn;
#endif
};

/* section flags */
#define HAS_SYMBOLS 1
#define RESOLVE_WARN 2
#define UNALLOCATED 4
#define LABELS_ARE_LOCAL 8
#define ABSOLUTE 16
#define PREVABS 32          /* saved ABSOLUTE-flag during RORG-block */
#define IN_RORG 64
#define NEAR_ADDRESSING 128
#define SECRSRVD (1L<<24)   /* bits 24..31 are reserved for output modules */

/* section description */
struct section {
  struct section *next;
  char *name;
  char *attr;
  atom *first;
  atom *last;
  taddr align;
  uint8_t pad[MAXPADBYTES];
  int padbytes;
  uint32_t flags;
  uint32_t memattr;  /* type of memory, used by some object formats */
  taddr org;
  taddr pc;
  unsigned long idx; /* usable by output module */
};

/* mnemonic description */
typedef struct mnemonic {
  char *name;
#if MAX_OPERANDS!=0
  int operand_type[MAX_OPERANDS];
#endif
  mnemonic_extension ext;
} mnemonic;

/* operand size flags (ORed with size in bits) */
#define OPSZ_BITS(x)	((x) & 0xff)
#define OPSZ_FLOAT      0x100  /* operand stored as floating point */
#define OPSZ_SWAP	0x200  /* operand stored with swapped bytes */

/* listing table */

#define MAXLISTSRC 120

struct listing {
  listing *next;
  source *src;
  int line;
  int error;
  atom *atom;
  section *sec;
  taddr pc;
  char txt[MAXLISTSRC];
};


extern listing *first_listing,*last_listing,*cur_listing;
extern int done,final_pass;
extern int warn_unalloc_ini_dat;
extern int listena,listformfeed,listlinesperpage,listnosyms;
extern int mnemonic_cnt;
extern int nocase,no_symbols,pic_check,secname_attr,exec_out,chklabels;
extern taddr inst_alignment;
extern hashtable *mnemohash;
extern source *cur_src;
extern section *current_section;
extern char *filename;
extern char *debug_filename;  /* usually an absolute C source file name */
extern char *inname,*outname,*listname;
extern char *output_format;
extern char emptystr[];
extern char vasmsym_name[];

extern unsigned long long taddrmask;
#define ULLTADDR(x) (((unsigned long long)x)&taddrmask)

/* provided by main assembler module */
extern int debug;

void leave(void);
void set_default_output_format(char *);
FILE *locate_file(char *,char *);
void include_source(char *);
source *new_source(char *,char *,size_t);
void end_source(source *);
void set_section(section *);
section *new_section(char *,char *,int);
section *new_org(taddr);
section *find_section(char *,char *);
void switch_section(char *,char *);
void switch_offset_section(char *,taddr);
void add_align(section *,taddr,expr *,int,unsigned char *);
section *default_section(void);
#if NOT_NEEDED
section *restore_section(void);
section *restore_org(void);
#endif
int end_rorg(void);
void try_end_rorg(void);
void start_rorg(taddr);
void print_section(FILE *,section *);
void new_include_path(char *);
void set_listing(int);
void set_list_title(char *,int);
void write_listing(char *);

#define setfilename(x) filename=(x)
#define getfilename() filename
#define setdebugname(x) debug_filename=(x)
#define getdebugname() debug_filename

/* provided by cpu.c */
extern int bitsperbyte;
extern int bytespertaddr;
extern mnemonic mnemonics[];
extern char *cpu_copyright;
extern char *cpuname;
extern int debug;

int init_cpu();
int cpu_args(char *);
char *parse_cpu_special(char *);
operand *new_operand();
int parse_operand(char *text,int len,operand *out,int requires);
#define PO_SKIP 2
#define PO_MATCH 1
#define PO_NOMATCH 0
#define PO_CORRUPT -1
size_t instruction_size(instruction *,section *,taddr);
dblock *eval_instruction(instruction *,section *,taddr);
dblock *eval_data(operand *,size_t,section *,taddr);
#if HAVE_INSTRUCTION_EXTENSION
void init_instruction_ext(instruction_ext *);
#endif
#if MAX_QUALIFIERS!=0
char *parse_instruction(char *,int *,char **,int *,int *);
int set_default_qualifiers(char **,int *);
#endif
#if HAVE_CPU_OPTS
void cpu_opts_init(section *);
void cpu_opts(void *);
void print_cpu_opts(FILE *,void *);
#endif

/* provided by syntax.c */
extern char *syntax_copyright;
extern char commentchar;
extern hashtable *dirhash;
extern char *defsectname;
extern char *defsecttype;

int init_syntax();
int syntax_args(char *);
void parse(void);
char *parse_macro_arg(struct macro *,char *,struct namelen *,struct namelen *);
int expand_macro(source *,char **,char *,int);
char *skip(char *);
char *skip_operand(char *);
void eol(char *);
char *const_prefix(char *,int *);
char *const_suffix(char *,char *);
char *get_local_label(char **);

/* provided by output_xxx.c */
#ifdef OUTTOS
extern int tos_hisoft_dri;
#endif
#ifdef OUTHUNK
extern int hunk_onlyglobal;
#endif

int init_output_test(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_elf(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_bin(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_srec(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_vobj(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_hunk(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_aout(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_tos(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
