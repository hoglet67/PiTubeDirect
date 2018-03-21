/* syntax.c  syntax module for vasm */
/* (c) in 2002-2018 by Frank Wille */

#include "vasm.h"

/* The syntax module parses the input (read_next_line), handles
   assembly-directives (section, data-storage etc.) and parses
   mnemonics. Assembly instructions are split up in mnemonic name,
   qualifiers and operands. new_inst returns a matching instruction,
   if one exists.
   Routines for creating sections and adding atoms to sections will
   be provided by the main module.
*/

char *syntax_copyright="vasm oldstyle syntax module 0.13e (c) 2002-2018 Frank Wille";
hashtable *dirhash;

static char textname[]=".text",textattr[]="acrx";
static char dataname[]=".data",dataattr[]="adrw";
static char rodataname[]=".rodata",rodataattr[]="adr";
static char bssname[]=".bss",bssattr[]="aurw";

char commentchar=';';
char *defsectname = textname;
char *defsecttype = "acrwx";

static char macname[] = ".mac";
static char macroname[] = ".macro";
static char eqname[] = ".eq";
static char equname[] = ".equ";
static char setname[] = ".set";

static char endmname[] = ".endmacro";
static char endrname[] = ".endrepeat";
static char reptname[] = ".rept";
static char repeatname[] = ".repeat";
static struct namelen endm_dirlist[] = {
  { 4,&endmname[1] }, { 6,&endmname[1] }, { 8,&endmname[1] }, { 0,0 }
};
static struct namelen rept_dirlist[] = {
  { 4,&reptname[1] }, { 6,&repeatname[1] }, { 0,0 }
};
static struct namelen endr_dirlist[] = {
  { 4,&endrname[1] }, { 6,&endrname[1] }, { 9,&endrname[1] }, { 0,0 }
};
static struct namelen dendm_dirlist[] = {
  { 5,&endmname[0] }, { 7,&endmname[0] }, { 9,&endmname[0] }, { 0,0 }
};
static struct namelen drept_dirlist[] = {
  { 5,&reptname[0] }, { 7,&repeatname[0] }, { 0,0 }
};
static struct namelen dendr_dirlist[] = {
  { 5,&endrname[0] }, { 7,&endrname[0] }, { 10,&endrname[0] }, { 0,0 }
};

static int dotdirs,autoexport,parse_end,igntrail,nocprefix,nointelsuffix;
static taddr orgmode = ~0;


char *skip(char *s)
{
  while (isspace((unsigned char )*s))
    s++;
  return s;
}


/* check for end of line, issue error, if not */
void eol(char *s)
{
  if (igntrail) {
    if (!ISEOL(s) && !isspace((unsigned char)*s))
      syntax_error(6);
  }
  else {
    s = skip(s);
    if (!ISEOL(s))
      syntax_error(6);
  }
}


char *exp_skip(char *s)
{
  if (!igntrail) {
    s = skip(s);
    if (*s == commentchar)
      *s = '\0';  /* rest of operand is ignored */
  }
  else if (isspace((unsigned char)*s) || *s==commentchar)
    *s = '\0';  /* rest of operand is ignored */
  return s;
}


static char *skip_oper(int instoper,char *s)
{
#ifdef VASM_CPU_Z80
  unsigned char lastuc = 0;
#endif
  int par_cnt = 0;
  char c = 0;

  for (;;) {
    s = exp_skip(s);
#ifdef VASM_CPU_Z80
    if (c)
      lastuc = toupper((unsigned char)*(s-1));
#endif
    c = *s;

    if (START_PARENTH(c))
      par_cnt++;
    else if (END_PARENTH(c)) {
      if (par_cnt>0)
        par_cnt--;
      else
        syntax_error(3);  /* too many closing parentheses */
    }
#ifdef VASM_CPU_Z80
    /* For the Z80 ignore ' behind a letter, as it may be a register */
    else if ((c=='\'' && (lastuc<'A' || lastuc>'Z')) || c=='\"')
#else
    else if (c=='\'' || c=='\"')
#endif
      s = skip_string(s,c,NULL) - 1;
    else if (instoper && OPERSEP_COMMA && c==',' && par_cnt==0)
      break;
    else if (instoper && OPERSEP_BLANK && isspace((unsigned char)c)
             && par_cnt==0)
      break;
    else if (!instoper && c==',' && par_cnt==0)
      break;
    else if (c == '\0')
      break;

    s++;
  }
  if(par_cnt != 0)
    syntax_error(4);  /* missing closing parentheses */
  return s;
}


char *skip_operand(char *s)
{
  return skip_oper(1,s);
}


char *my_skip_macro_arg(char *s)
{
  if (*s == '\\')
    s++;  /* leading \ in argument list is optional */
  return skip_identifier(s);
}


#define handle_data(a,b) handle_data_offset(a,b,0)

static void handle_data_offset(char *s,int size,int offset)
{
  for (;;) {
    char *opstart = s;
    operand *op;
    dblock *db = NULL;

    if (size==8 && (*s=='\"' || *s=='\'')) {
      if (db = parse_string(&opstart,*s,8)) {
#if defined(VASM_CPU_650X) || defined(VASM_CPU_Z80) || defined(VASM_CPU_6800)
        if (offset != 0) {
          int i;

          for (i=0; i<db->size; i++)
            db->data[i] = db->data[i] + offset;
        }
#endif
        add_atom(0,new_data_atom(db,1));
        s = opstart;
      }
    }
    if (!db) {
      op = new_operand();
      s = skip_oper(0,s);
      if (parse_operand(opstart,s-opstart,op,DATA_OPERAND(size))) {
        atom *a;

#if defined(VASM_CPU_650X) || defined(VASM_CPU_Z80) || defined(VASM_CPU_6800)
        if (offset != 0)
          op->value = make_expr(ADD,number_expr(offset),op->value);
#endif
        a = new_datadef_atom(abs(size),op);
        a->align = 1;
        add_atom(0,a);
      }
      else
        syntax_error(8);  /* invalid data operand */
    }

    s = skip(s);
    if (*s == ',') {
      s = skip(s+1);
    }
    else if (*s == commentchar) {
      break;
    }
    else if (*s) {
      syntax_error(9);  /* , expected */
      return;
    }
    else
      break;
  }

  eol(s);
}


static void handle_text(char *s)
{
  char *opstart = s;
  operand *op;
  dblock *db = NULL;

  if (db = parse_string(&opstart,*s,8)) {
    add_atom(0,new_data_atom(db,1));
    s = opstart;
  }
  if (!db) {
    op = new_operand();
    s = skip_oper(0,s);
    if (parse_operand(opstart,s-opstart,op,DATA_OPERAND(8))) {
      atom *a;

      a = new_datadef_atom(8,op);
      a->align = 1;
      add_atom(0,a);
    }
    else
      syntax_error(8);  /* invalid data operand */
  }
  eol(s);
}


static void handle_d8(char *s)
{
  handle_data(s,8);
}


static void handle_d16(char *s)
{
  handle_data(s,16);
}


static void handle_d24(char *s)
{
  handle_data(s,24);
}


static void handle_d32(char *s)
{
  handle_data(s,32);
}


#if defined(VASM_CPU_650X) || defined(VASM_CPU_Z80) || defined(VASM_CPU_6800)
static void handle_d8_offset(char *s)
{
  taddr offs = parse_constexpr(&s);

  s = skip(s);
  if (*s == ',') {
    s = skip(s+1);
    handle_data_offset(s,8,offs);
  }
  else
    syntax_error(9);  /* , expected */
}
#endif


static void do_alignment(taddr align,expr *offset)
{
  atom *a = new_space_atom(offset,1,0);

  a->align = align;
  add_atom(0,a);
}


static void handle_align(char *s)
{
  taddr a = parse_constexpr(&s);

  if (a > 63)
    syntax_error(21);  /* bad alignment */
  do_alignment(1LL<<a,number_expr(0));
  eol(s);
}


static void handle_even(char *s)
{
  do_alignment(2,number_expr(0));
  eol(s);
}


static void do_space(int size,expr *cnt,expr *fill)
{
  add_atom(0,new_space_atom(cnt,size>>3,fill));
}


static void handle_space(char *s,int size)
{
  expr *cnt,*fill=0;

  cnt = parse_expr_tmplab(&s);
  s = skip(s);
  if (*s == ',') {
    s = skip(s+1);
    fill = parse_expr_tmplab(&s);
  }
  do_space(size,cnt,fill);
  eol(s);
}


static void handle_fixedspc1(char *s)
{
  do_space(8,number_expr(1),0);
  eol(s);
}


static void handle_fixedspc2(char *s)
{
  do_space(8,number_expr(2),0);
  eol(s);
}


static void handle_spc8(char *s)
{
  handle_space(s,8);
}


static void handle_spc16(char *s)
{
  handle_space(s,16);
}


#if 0
static void handle_spc24(char *s)
{
  handle_space(s,24);
}


static void handle_spc32(char *s)
{
  handle_space(s,32);
}
#endif


static void handle_string(char *s)
{
  handle_data(s,8);  
  add_atom(0,new_space_atom(number_expr(1),1,0));  /* terminating zero */
}


static void handle_end(char *s)
{
  parse_end = 1;
  eol(s);
}


static void handle_fail(char *s)
{
  add_atom(0,new_assert_atom(NULL,NULL,mystrdup(s)));
}


static void handle_org(char *s)
{
  if (*s == current_pc_char) {
    char *s2 = skip(s+1);

    if (*s2++ == '+') {
      handle_space(skip(s2),8);  /*  "* = * + <expr>" to reserves bytes */
      return;
    }
  }
  set_section(new_org(parse_constexpr(&s)));
  eol(s);
}


static void handle_rorg(char *s)
{
  start_rorg(parse_constexpr(&s));
  eol(s);
}
  

static void handle_rend(char *s)
{
  if (end_rorg())
    eol(s);
}


static void handle_roffs(char *s)
{
  add_atom(0,new_roffs_atom(parse_expr_tmplab(&s)));
}
  

static void handle_section(char *s)
{
  char *name,*attr;

  if (!(name=parse_name(&s)))
    return;
  if (*s==',') {
    s = skip(s+1);
    attr = s;
    if (*s!= '\"')
      syntax_error(7,'\"');  /* " expected */
    else
      s++;
    attr = s;
    while (*s&&*s!='\"')
      s++;    
    attr = cnvstr(attr,s-attr);
    s = skip(s+1);
  }
  else {
    if (!strcmp(name,textname)) attr = textattr;
    if (!strcmp(name,dataname)) attr = dataattr;
    if (!strcmp(name,rodataname)) attr = rodataattr;
    if (!strcmp(name,bssname)) attr = bssattr;
    else attr = defsecttype;
  }

  new_section(name,attr,1);
  switch_section(name,attr);
  eol(s);
}


static void do_binding(char *s,int bind)
{
  symbol *sym;
  char *name;

  while (1) {
    if (!(name=parse_identifier(&s)))
      return;
    sym = new_import(name);
    myfree(name);
    if (sym->flags&(EXPORT|WEAK|LOCAL)!=0 &&
        sym->flags&(EXPORT|WEAK|LOCAL)!=bind)
      general_error(62,sym->name,get_bind_name(sym)); /* binding already set */
    else
      sym->flags |= bind;
    s = skip(s);
    if (*s != ',')
      break;
    s = skip(s+1);
  }
  eol(s);
}


static void handle_global(char *s)
{
  do_binding(s,EXPORT);
}


static void handle_weak(char *s)
{
  do_binding(s,WEAK);
}


static void handle_local(char *s)
{
  do_binding(s,LOCAL);
}


static void ifdef(char *s,int b)
{
  char *name;
  symbol *sym;
  int result;

  if (!(name = parse_symbol(&s))) {
    syntax_error(10);  /* identifier expected */
    return;
  }
  if (sym = find_symbol(name))
    result = sym->type != IMPORT;
  else
    result = 0;
  myfree(name);
  cond_if(result == b);
  eol(s);
}


static void ifused(char *s, int b)
{
  char *name;
  symbol *sym;
  int result;

  if (!(name = parse_identifier(&s))) {
      syntax_error(10);  /* identifier expected */
      return;
  }

  if (sym = find_symbol(name)) {
    if (sym->type != IMPORT) {
      syntax_error(22,name);
      result = 0;
    }
    else
      result = 1;
  }
  else
    result = 0;

  myfree(name);
  cond_if(result == b);
  eol(s);
}


static void handle_ifused(char *s)
{
  ifused(s,1);
}


static void handle_ifnused(char *s)
{
  ifused(s,0);
}


static void handle_ifd(char *s)
{
  ifdef(s,1);
}


static void handle_ifnd(char *s)
{
  ifdef(s,0);
}


static char *ifexp(char *s,int c)
{
  expr *condexp = parse_expr_tmplab(&s);
  taddr val;
  int b;

  if (eval_expr(condexp,&val,NULL,0)) {
    switch (c) {
      case 0: b = val == 0; break;
      case 1: b = val != 0; break;
      case 2: b = val > 0; break;
      case 3: b = val >= 0; break;
      case 4: b = val < 0; break;
      case 5: b = val <= 0; break;
      default: ierror(0); break;
    }
  }
  else {
    general_error(30);  /* expression must be constant */
    b = 0;
  }
  cond_if(b);
  free_expr(condexp);
  return s;
}


static void handle_ifeq(char *s)
{
  eol(ifexp(s,0));
}


static void handle_ifne(char *s)
{
  eol(ifexp(s,1));
}


static void handle_ifgt(char *s)
{
  eol(ifexp(s,2));
}


static void handle_ifge(char *s)
{
  eol(ifexp(s,3));
}


static void handle_iflt(char *s)
{
  eol(ifexp(s,4));
}


static void handle_ifle(char *s)
{
  eol(ifexp(s,5));
}


static void handle_else(char *s)
{
  eol(s);
  cond_skipelse();
}


static void handle_endif(char *s)
{
  eol(s);
  cond_endif();
}


static void handle_assert(char *s)
{
  char *expstr,*msgstr;
  size_t explen;
  expr *aexp;
  atom *a;

  expstr = skip(s);
  aexp = parse_expr(&s);
  explen = s - expstr;
  s = skip(s);
  if (*s == ',') {
    s = skip(s+1);
    msgstr = parse_name(&s);
  }
  else
    msgstr = NULL;

  a = new_assert_atom(aexp,cnvstr(expstr,explen),msgstr);
  add_atom(0,a);
}


static void handle_incdir(char *s)
{
  char *name;

  if (name = parse_name(&s))
    new_include_path(name);
  eol(s);
}


static void handle_include(char *s)
{
  char *name;

  if (name = parse_name(&s)) {
    eol(s);
    include_source(name);
  }
}


static void handle_incbin(char *s)
{
  char *name;
  long delta = 0;
  unsigned long nbbytes = 0;

  if (name = parse_name(&s)) {
    s = skip(s);
    if (*s == ',') {
      s = skip(s+1);
      delta = parse_constexpr(&s);
      if (delta < 0)
        delta = 0;
      s = skip(s);
      if (*s == ',') {
        s = skip(s+1);
        nbbytes = parse_constexpr(&s);
      }
    }
    eol(s);
    include_binary_file(name,delta,nbbytes);
  }
}


static void handle_rept(char *s)
{
  utaddr cnt = parse_constexpr(&s);

  eol(s);
  new_repeat((int)cnt,NULL,NULL,
             dotdirs?drept_dirlist:rept_dirlist,
             dotdirs?dendr_dirlist:endr_dirlist);
}


static void handle_endr(char *s)
{
  syntax_error(12,&endrname[1],&repeatname[1]);  /* unexpected endr without rept */
}


static void handle_macro(char *s)
{
  char *name;

  if (name = parse_identifier(&s)) {
    s = skip(s);
    if (*s == ',') {  /* named macro arguments are given? */
      s++;
    }
    else {
      eol(s);
      s = NULL;
    }
    new_macro(name,dotdirs?dendm_dirlist:endm_dirlist,s);
    myfree(name);
  }
  else
    syntax_error(10);  /* identifier expected */
}


static void handle_endm(char *s)
{
  syntax_error(12,&endmname[1],&macroname[1]);  /* unexpected endm without macro */
}


static void handle_defc(char *s)
{
  char *name;

  s = skip(s);
  name = parse_identifier(&s);
  if ( name != NULL ) {
    s = skip(s);
    if ( *s == '=' ) {
      s = skip(s+1);
      new_abs(name,parse_expr_tmplab(&s));
    }
    myfree(name);
  }
  else
    syntax_error(10);
}


static void handle_list(char *s)
{
  set_listing(1);
}

static void handle_nolist(char *s)
{
  set_listing(0);
}

static void handle_listttl(char *s)
{
  /* set listing file title */
}

static void handle_listsubttl(char *s)
{
  /* set listing file sub-title */
}

static void handle_listpage(char *s)
{
  /* new listing page */
}

static void handle_listspace(char *s)
{
  /* insert listing space */
}


static void handle_struct(char *s)
{
  char *name;

  if (name = parse_identifier(&s)) {
    s = skip(s);
    eol(s);
    if (new_structure(name))
      current_section->flags |= LABELS_ARE_LOCAL;
    myfree(name);
  }
  else
    syntax_error(10);  /* identifier expected */
}


static void handle_endstruct(char *s)
{
  section *prevsec;
  symbol *szlabel;

  if (end_structure(&prevsec)) {
    /* create the structure name as label defining the structure size */
    current_section->flags &= ~LABELS_ARE_LOCAL;
    szlabel = new_labsym(0,current_section->name);
    add_atom(0,new_label_atom(szlabel));
    /* end structure declaration by switching to previous section */
    set_section(prevsec);
  }
  eol(s);
}


struct {
  char *name;
  void (*func)(char *);
} directives[] = {
  "org",handle_org,
  "rorg",handle_rorg,
  "rend",handle_rend,
  "phase",handle_rorg,
  "dephase",handle_rend,
  "roffs",handle_roffs,
  "align",handle_align,
  "even",handle_even,
  "byte",handle_d8,
  "db",handle_d8,
  "dfb",handle_d8,
  "defb",handle_d8,
  "asc",handle_d8,
  "data",handle_d8,
  "defm",handle_text,
  "text",handle_text,
  "wor",handle_d16,
  "word",handle_d16,
  "addr",handle_d16,
  "dw",handle_d16,
  "dfw",handle_d16,
  "defw",handle_d16,
  "dd",handle_d32,
#if defined(VASM_CPU_650X) || defined(VASM_CPU_Z80) || defined(VASM_CPU_6800)
  "abyte",handle_d8_offset,
#endif
  "ds",handle_spc8,
  "dsb",handle_spc8,
  "fill",handle_spc8,
  "reserve",handle_spc8,
  "spc",handle_spc8,
  "dsw",handle_spc16,
  "blk",handle_spc8,
  "blkw",handle_spc16,
  "dc",handle_spc8,
  "byt",handle_fixedspc1,
  "wrd",handle_fixedspc2,
  "assert",handle_assert,
#if defined(VASM_CPU_TR3200) /* Clash with IFxx instructions of TR3200 cpu */
  "if_def",handle_ifd,
  "if_ndef",handle_ifnd,
  "if_eq",handle_ifeq,
  "if_ne",handle_ifne,
  "if_gt",handle_ifgt,
  "if_ge",handle_ifge,
  "if_lt",handle_iflt,
  "if_le",handle_ifle,
  "if_used",handle_ifused,
  "if_nused",handle_ifnused,
#else
  "ifdef",handle_ifd,
  "ifndef",handle_ifnd,
  "ifd",handle_ifd,
  "ifnd",handle_ifnd,
  "ifeq",handle_ifeq,
  "ifne",handle_ifne,
  "ifgt",handle_ifgt,
  "ifge",handle_ifge,
  "iflt",handle_iflt,
  "ifle",handle_ifle,
  "ifused",handle_ifused,
  "ifnused",handle_ifnused,
#endif
  "if",handle_ifne,
  "else",handle_else,
  "el",handle_else,
  "endif",handle_endif,
#if !defined(VASM_CPU_Z80) && !defined(VASM_CPU_6800)
  "ei",handle_endif,  /* Clashes with z80 opcode */
#endif
  "incbin",handle_incbin,
  "mdat",handle_incbin,
  "incdir",handle_incdir,
  "include",handle_include,
  "rept",handle_rept,
  "repeat",handle_rept,
  "endr",handle_endr,
  "endrep",handle_endr,
  "endrepeat",handle_endr,
  "mac",handle_macro,
  "macro",handle_macro,
  "endm",handle_endm,
  "endmac",handle_endm,
  "endmacro",handle_endm,
  "end",handle_end,
  "fail",handle_fail,
  "section",handle_section,
  "binary",handle_incbin,
  "defs",handle_spc8,
  "defp",handle_d24,
  "defl",handle_d32,
  "defc",handle_defc,
  "xdef",handle_global,
  "xref",handle_global,
  "lib",handle_global,
  "xlib",handle_global,
  "global",handle_global,
  "extern",handle_global,
  "local",handle_local,
  "weak",handle_weak,
  "ascii",handle_string,
  "asciiz",handle_string,
  "string",handle_string,
  "list",handle_list,
  "nolist",handle_nolist,
  "struct",handle_struct,
  "structure",handle_struct,
  "endstruct",handle_endstruct,
  "endstructure",handle_endstruct,
  "rmb",handle_spc8,
  "fcc",handle_text,
  "fcb",handle_d8,
  "fdb",handle_d16,
  "bsz",handle_spc8,
  "zmb",handle_spc8,
  "nam",handle_listttl,
  "subttl",handle_listsubttl,
  "page",handle_listpage,
  "space",handle_listspace
};

int dir_cnt = sizeof(directives) / sizeof(directives[0]);


/* checks for a valid directive, and return index when found, -1 otherwise */
static int check_directive(char **line)
{
  char *s,*name;
  hashdata data;

  s = skip(*line);
  if (!ISIDSTART(*s))
    return -1;
  name = s++;
  while (ISIDCHAR(*s))
    s++;
  if (*name=='.' && dotdirs)
    name++;
  if (!find_namelen_nc(dirhash,name,s-name,&data))
    return -1;
  *line = s;
  return data.idx;
}


/* Handles assembly directives; returns non-zero if the line
   was a directive. */
static int handle_directive(char *line)
{
  int idx = check_directive(&line);

  if (idx >= 0) {
    directives[idx].func(skip(line));
    return 1;
  }
  return 0;
}


static int oplen(char *e,char *s)
{
  while(s!=e&&isspace((unsigned char)e[-1]))
    e--;
  return e-s;
}


/* When a structure with this name exists, insert its atoms and either
   initialize with new values or accept its default values. */
static int execute_struct(char *name,int name_len,char *s)
{
  section *str;
  atom *p;

  str = find_structure(name,name_len);
  if (str == NULL)
    return 0;

  for (p=str->first; p; p=p->next) {
    atom *new;
    char *opp;
    int opl;

    if (p->type==DATA || p->type==SPACE || p->type==DATADEF) {
      opp = s = skip(s);
      s = skip_oper(0,s);
      opl = oplen(s,opp);

      if (opl > 0) {
        /* initialize this atom with a new expression */

        if (p->type == DATADEF) {
          /* parse a new data operand of the declared bitsize */
          operand *op;

          op = new_operand();
          if (parse_operand(opp,opl,op,
                            DATA_OPERAND(p->content.defb->bitsize))) {
            new = new_datadef_atom(p->content.defb->bitsize,op);
            new->align = p->align;
            add_atom(0,new);
          }
          else
            syntax_error(8);  /* invalid data operand */
        }
        else if (p->type == SPACE) {
          /* parse the fill expression for this space */
          new = clone_atom(p);
          new->content.sb = new_sblock(p->content.sb->space_exp,
                                       p->content.sb->size,
                                       parse_expr_tmplab(&opp));
          new->content.sb->space = p->content.sb->space;
          add_atom(0,new);
        }
        else {
          /* parse constant data - probably a string, or a single constant */
          dblock *db;

          db = new_dblock();
          db->size = p->content.db->size;
          db->data = db->size ? mycalloc(db->size) : NULL;
          if (db->data) {
            if (*opp=='\"' || *opp=='\'') {
              dblock *strdb;

              strdb = parse_string(&opp,*opp,8);
              if (strdb->size) {
                if (strdb->size > db->size)
                  syntax_error(24,strdb->size-db->size);  /* cut last chars */
                memcpy(db->data,strdb->data,
                       strdb->size > db->size ? db->size : strdb->size);
                myfree(strdb->data);
              }
              myfree(strdb);
            }
            else {
              taddr val = parse_constexpr(&opp);
              void *p;

              if (db->size > sizeof(taddr) && BIGENDIAN)
                p = db->data + db->size - sizeof(taddr);
              else
                p = db->data;
              setval(BIGENDIAN,p,sizeof(taddr),val);
            }
          }
          add_atom(0,new_data_atom(db,p->align));
        }
      }
      else {
        /* empty: use default values from original atom */
        add_atom(0,clone_atom(p));
      }

      s = skip(s);
      if (*s == ',')
        s++;
    }
    else if (p->type == INSTRUCTION)
      syntax_error(23);  /* skipping instruction in struct init */

    /* other atoms are silently ignored */
  }

  eol(s);
  return 1;
}


static char *parse_label_or_pc(char **start)
{
  char *s,*name;

  name = parse_labeldef(start,0);
  s = skip(*start);
  if (name==NULL && *s==current_pc_char && !ISIDCHAR(*(s+1))) {
    name = cnvstr(s,1);
    s = skip(s+1);
  }
  if (name)
    *start = s;
  return name;
}


#ifdef STATEMENT_DELIMITER
static char *read_next_statement(void)
{
  static char *s = NULL;
  char *line,c;

  if (s == NULL) {
    char *lab;

    s = line = read_next_line();
    if (s == NULL)
      return NULL;  /* no more lines in source */

    /* skip label field and possible statement delimiters therein */
    if (lab = parse_label_or_pc(&s))
      myfree(lab);
  }
  else {
    /* make the new statement start with a blank - there is no label field */
    *s = ' ';
    line = s++;
  }

  /* find next statement delimiter in line buffer */
  for (;;) {
#ifdef VASM_CPU_Z80
    unsigned char lastuc;
#endif

    c = *s;
#ifdef VASM_CPU_Z80
    /* For the Z80 ignore ' behind a letter, as it may be a register */
    lastuc = toupper((unsigned char)*(s-1));
    if ((c=='\'' && (lastuc<'A' || lastuc>'Z')) || c=='\"') {
#else
    if (c=='\'' || c=='\"') {
#endif
      s = skip_string(s,c,NULL);
    }
    else if (c == STATEMENT_DELIMITER) {
      *s = '\0';  /* terminate the statement here temporarily */
      break;
    }
    else if (c=='\0' || c==commentchar) {
      s = NULL;  /* ignore delimiters in rest of line */
      break;
    }
    else
      s++;
  }
  return line;
}
#endif


void parse(void)
{
  char *s,*line,*inst,*labname;
  char *ext[MAX_QUALIFIERS?MAX_QUALIFIERS:1];
  char *op[MAX_OPERANDS];
  int ext_len[MAX_QUALIFIERS?MAX_QUALIFIERS:1];
  int op_len[MAX_OPERANDS];
  int ext_cnt,op_cnt,inst_len;
  instruction *ip;

#ifdef STATEMENT_DELIMITER
  while (line = read_next_statement()) {
#else
  while (line = read_next_line()) {
#endif
    if (parse_end)
      continue;

    if (!cond_state()) {
      /* skip source until ELSE or ENDIF */
      int idx;

      s = line;
      if (labname = parse_label_or_pc(&s))
        myfree(labname);
      idx = check_directive(&s);
      if (idx >= 0) {
        if (!strncmp(directives[idx].name,"if",2))
          cond_skipif();
        else if (directives[idx].func == handle_else)
          cond_else();
        else if (directives[idx].func == handle_endif)
          cond_endif();
      }
      continue;
    }

    s = line;
    if (labname = parse_label_or_pc(&s)) {
      /* we have found a global or local label, or current-pc character */
      symbol *label,*labsym;
      int equlen = 0;

      if (!strnicmp(s,equname+!dotdirs,3+dotdirs) &&
          isspace((unsigned char)*(s+3+dotdirs)))
        equlen = 3+dotdirs;
      else if (!strnicmp(s,eqname+!dotdirs,2+dotdirs) &&
               isspace((unsigned char)*(s+2+dotdirs)))
        equlen = 2+dotdirs;
      else if (*s == '=')
        equlen = 1;

      if (equlen) {
        /* found a kind of equate directive */
        if (*labname == current_pc_char) {
          handle_org(skip(s+equlen));
          continue;
        }
        else {
          s = skip(s+equlen);
          label = new_equate(labname,parse_expr_tmplab(&s));
        }
      }
      else if (!strnicmp(s,setname+!dotdirs,3+dotdirs) &&
               isspace((unsigned char)*(s+3+dotdirs))) {
        /* SET allows redefinitions */
        if (*labname == current_pc_char) {
          syntax_error(10);  /* identifier expected */
        }
        else {
          s = skip(s+3+dotdirs);
          label = new_abs(labname,parse_expr_tmplab(&s));
        }
      }
      else if (!strnicmp(s,macname+!dotdirs,3+dotdirs) &&
               (isspace((unsigned char)*(s+3+dotdirs)) ||
                *(s+3+dotdirs)=='\0') ||
               !strnicmp(s,macroname+!dotdirs,5+dotdirs) &&
               (isspace((unsigned char)*(s+5+dotdirs)) ||
                *(s+5+dotdirs)=='\0')) {
        char *params = skip(s + (*(s+3+dotdirs)=='r'?5+dotdirs:3+dotdirs));

        s = line;
        myfree(labname);
        if (!(labname = parse_identifier(&s)))
          ierror(0);
        new_macro(labname,dotdirs?dendm_dirlist:endm_dirlist,params);
        myfree(labname);
        continue;
      }
      else {
        /* it's just a label */
        label = new_labsym(0,labname);
        add_atom(0,new_label_atom(label));
      }

      if (!is_local_label(labname) && autoexport)
          label->flags |= EXPORT;
      myfree(labname);
    }

    /* check for directives first */
    s = skip(s);
    if (*s==commentchar)
      continue;

    s = parse_cpu_special(s);
    if (ISEOL(s))
      continue;

    if (*s==current_pc_char && *(s+1)=='=') {   /* "*=" org directive */ 
      handle_org(skip(s+2));
      continue;
    }
    if (handle_directive(s))
      continue;

    s = skip(s);
    if (ISEOL(s))
      continue;

    /* read mnemonic name */
    inst = s;
    ext_cnt = 0;
    if (!ISIDSTART(*s)) {
      syntax_error(10);  /* identifier expected */
      continue;
    }
#if MAX_QUALIFIERS==0
    while (*s && !isspace((unsigned char)*s))
      s++;
    inst_len = s - inst;
#else
    s = parse_instruction(s,&inst_len,ext,ext_len,&ext_cnt);
#endif
    if (!isspace((unsigned char)*s) && *s!='\0')
      syntax_error(2);  /* no space before operands */
    s = skip(s);

    if (execute_macro(inst,inst_len,ext,ext_len,ext_cnt,s))
      continue;
    if (execute_struct(inst,inst_len,s))
      continue;

    /* read operands, terminated by comma or blank (unless in parentheses) */
    op_cnt = 0;
    while (!ISEOL(s) && op_cnt<MAX_OPERANDS) {
      op[op_cnt] = s;
      s = skip_oper(1,s);
      op_len[op_cnt] = oplen(s,op[op_cnt]);
#if !ALLOW_EMPTY_OPS
      if (op_len[op_cnt] <= 0)
        syntax_error(5);  /* missing operand */
      else
#endif
        op_cnt++;

      if (igntrail) {
        if (*s != ',')
          break;
        s++;
      }
      else {
        s = skip(s);
        if (OPERSEP_COMMA) {
          if (*s == ',')
            s = skip(s+1);
          else if (!(OPERSEP_BLANK))
            break;
        }
      }
    }
    eol(s);

    ip = new_inst(inst,inst_len,op_cnt,op,op_len);

#if MAX_QUALIFIERS>0
    if (ip) {
      int i;

      for (i=0; i<ext_cnt; i++)
        ip->qualifiers[i] = cnvstr(ext[i],ext_len[i]);
      for(; i<MAX_QUALIFIERS; i++)
        ip->qualifiers[i] = NULL;
    }
#endif

    if (ip)
      add_atom(0,new_inst_atom(ip));
  }

  cond_check();
}


/* parse next macro argument */
char *parse_macro_arg(struct macro *m,char *s,
                      struct namelen *param,struct namelen *arg)
{
  arg->len = 0;  /* cannot select specific named arguments */
  param->name = s;

  if (*s=='\"' || *s=='\'') {
    s = skip_string(s,*s,NULL);
    param->len = s - param->name;
  }
  else {
    s = skip_operand(s);
    param->len = s - param->name;
  }

  return s;
}


/* expands arguments and special escape codes into macro context */
int expand_macro(source *src,char **line,char *d,int dlen)
{
  int nc = 0;
  int n;
  char *s = *line;
  char *end;

  if (*s++ == '\\') {
    /* possible macro expansion detected */

    if (*s == '\\') {
      if (dlen >= 1) {
        *d++ = *s++;
        if (esc_sequences) {
          if (dlen >= 2) {
            *d++ = '\\';  /* make it a double \ again */
            nc = 2;
          }
          else
            nc = -1;
        }
        else
          nc = 1;
      }
      else
        nc = -1;
    }

    else if (*s == '@') {
      /* \@: insert a unique id */
      nc = snprintf(d,dlen,"_%06lu",src->id);
      s++;
      if (nc >= dlen)
        nc = -1;
    }
    else if (*s=='(' && *(s+1)==')') {
      /* \() is just skipped, useful to terminate named macro parameters */
      nc = 0;
      s += 2;
    }
    else if (*s == '<') {
      /* \<symbol> : insert absolute unsigned symbol value */
      char *name;
      symbol *sym;
      taddr val;

      s++;
      if (name = parse_symbol(&s)) {
        if ((sym = find_symbol(name)) && sym->type==EXPRESSION) {
          if (eval_expr(sym->expr,&val,NULL,0))
            nc = sprintf(d,"%lu",(unsigned long)(uint32_t)val);
        }
        myfree(name);
        if (*s++!='>' || nc<0) {
          syntax_error(11);  /* invalid numeric expansion */
          return 0;
        }
      }
      else {
        syntax_error(10);  /* identifier expected */
        return 0;
      }
    }
    else if (isdigit((unsigned char)*s)) {
      /* \1..\9,\0 : insert macro parameter 1..9,10 */
      nc = copy_macro_param(src,*s=='0'?0:*s-'1',d,dlen);
      s++;
    }
    else if ((end = skip_identifier(s)) != NULL) {
      if ((n = find_macarg_name(src,s,end-s)) >= 0) {
        /* \argname: insert named macro parameter n */
        nc = copy_macro_param(src,n,d,dlen);
        s = end;
      }
    }

    if (nc >= 0)
      *line = s;  /* update line pointer when expansion took place */
  }

  return nc;  /* number of chars written to line buffer, -1: out of space */
}


static int intel_suffix(char *s)
/* check for constants with h, d, o, q or b suffix */
{
  int base,lastbase;
  char c;
  
  base = 2;
  while (isxdigit((unsigned char)*s)) {
    lastbase = base;
    switch (base) {
      case 2:
        if (*s <= '1') break;
        base = 8;
      case 8:
        if (*s <= '7') break;
        base = 10;
      case 10:
        if (*s <= '9') break;
        base = 16;
    }
    s++;
  }

  c = tolower((unsigned char)*s);
  if (c == 'h')
    return 16;
  if ((c=='o' || c=='q') && base<=8)
    return 8;

  c = tolower((unsigned char)*(s-1));
  if (c=='d' && lastbase<=10)
    return 10;
  if (c=='b' && lastbase<=2)
    return 2;

  return 0;
}


char *const_prefix(char *s,int *base)
{
  if (isdigit((unsigned char)*s)) {
    if (!nointelsuffix && (*base = intel_suffix(s)))
      return s;
    if (!nocprefix) {
      if (*s == '0') {
        if (s[1]=='x' || s[1]=='X'){
          *base = 16;
          return s+2;
        }
        if (s[1]=='b' || s[1]=='B'){
          *base = 2;
          return s+2;
        }    
        *base = 8;
        return s;
      } 
      else if (s[1]=='#' && *s>='2' && *s<='9') {
        *base = *s & 0xf;
        return s+2;
      }
    }
    *base = 10;
    return s;
  }

  if (*s=='$' && isxdigit((unsigned char)s[1])) {
    *base = 16;
    return s+1;
  }
#if defined(VASM_CPU_Z80)
  if ((*s=='&' || *s=='#') && isxdigit((unsigned char)s[1])) {
    *base = 16;
    return s+1;
  }
#endif
  if (*s=='@') {
#if defined(VASM_CPU_Z80)
    *base = 2;
#else
    *base = 8;
#endif
    return s+1;
  }
  if (*s == '%') {
    *base = 2;
    return s+1;
  }
  *base = 0;
  return s;
}


char *const_suffix(char *start,char *end)
{
  if (intel_suffix(start))
    return end+1;

  return end;
}


static char *skip_local(char *p)
{
  char *s;

  if (ISIDSTART(*p) || isdigit((unsigned char)*p)) {  /* may start with digit */
    s = p++;
    while (ISIDCHAR(*p))
      p++;
  }
  else
    p = NULL;

  return p;
}


char *get_local_label(char **start)
/* Local labels start with a '.' or end with '$': "1234$", ".1" */
{
  char *s,*p,*name;

  name = NULL;
  s = *start;
  p = skip_local(s);

  if (p!=NULL && *p=='.' && ISIDCHAR(*(p+1)) &&
      ISIDSTART(*s) && *s!='.' && *(p-1)!='$') {
    /* skip local part of global.local label */
    s = p + 1;
    p = skip_local(p);
    name = make_local_label(*start,(s-1)-*start,s,p-s);
    *start = skip(p);
  }
  else if (p!=NULL && p>(s+1) && *s=='.') {  /* .label */
    s++;
    name = make_local_label(NULL,0,s,p-s);
    *start = skip(p);
  }
  else if (p!=NULL && p>s && *p=='$') { /* label$ */
    p++;
    name = make_local_label(NULL,0,s,p-s);
    *start = skip(p);
  }

  return name;
}


int init_syntax()
{
  size_t i;
  hashdata data;

  dirhash = new_hashtable(0x200); /* @@@ */
  for (i=0; i<dir_cnt; i++) {
    data.idx = i;
    add_hashentry(dirhash,directives[i].name,data);
  }

  cond_init();
  set_internal_abs(REPTNSYM,-1); /* reserve the REPTN symbol */
  current_pc_char = '*';

  if (orgmode != ~0)
    set_section(new_org(orgmode));
  return 1;
}


int syntax_args(char *p)
{
  if (!strcmp(p,"-dotdir")) {
    dotdirs = 1;
    return 1;
  }
  else if (!strcmp(p,"-autoexp")) {
    autoexport = 1;
    return 1;
  }
  else if (!strncmp(p,"-org=",5)) {
    orgmode = atoi(p+5);
    return 1;
  }
  else if (OPERSEP_COMMA && !strcmp(p,"-i")) {
    igntrail = 1;
    return 1;
  }
  else if (!strcmp(p,"-noc")) {
    nocprefix = 1;
    return 1;
  }
  else if (!strcmp(p,"-noi")) {
    nointelsuffix = 1;
    return 1;
  }
  return 0;
}
