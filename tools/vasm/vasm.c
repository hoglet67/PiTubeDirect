/* vasm.c  main module for vasm */
/* (c) in 2002-2016 by Volker Barthelmann */

#include <stdlib.h>
#include <stdio.h>

#include "vasm.h"
#include "stabs.h"

#define _VER "vasm 1.7g"
char *copyright = _VER " (c) in 2002-2016 Volker Barthelmann";
#ifdef AMIGA
static const char *_ver = "$VER: " _VER " " __AMIGADATE__ "\r\n";
#endif

#define SRCREADINC (64*1024)  /* extend buffer in these steps when reading */

/* The resolver will run another pass over the current section as long as any
   label location or atom size has changed. It gives up at MAXPASSES, which
   hopefully will never happen.
   During the first FASTOPTPHASE passes all instructions of a section will be
   optimized at the same time. After that the resolver enters a safe mode,
   where only a single instruction per pass is changed. */
#define MAXPASSES 1000
#define FASTOPTPHASE 50

source *cur_src=NULL;
char *filename,*debug_filename;
section *current_section;
char *inname,*outname,*listname;
int secname_attr;
int unnamed_sections;
int ignore_multinc;
int nocase;
int no_symbols;
int pic_check;
int done,final_pass,debug;
int exec_out;
int chklabels;
int listena,listformfeed=1,listlinesperpage=40,listnosyms;
listing *first_listing,*last_listing,*cur_listing;
struct stabdef *first_nlist,*last_nlist;
char *output_format="test";
unsigned long long taddrmask;
char emptystr[]="";
char vasmsym_name[]="__VASM";

static int produce_listing;
static char **listtitles;
static int *listtitlelines;
static int listtitlecnt;

static FILE *outfile=NULL;

static int depend;
#define DEPEND_LIST     1
#define DEPEND_MAKE     2
struct deplist {
  struct deplist *next;
  char *filename;
};
static struct deplist *first_depend,*last_depend;

static section *first_section,*last_section;
#if NOT_NEEDED
static section *prev_sec=NULL,*prev_org=NULL;
#endif

/* MNEMOHTABSIZE should be defined by cpu module */
#ifndef MNEMOHTABSIZE
#define MNEMOHTABSIZE 0x1000
#endif
hashtable *mnemohash;

static int verbose=1,auto_import=1;
static struct include_path *first_incpath=NULL;
static struct include_path *first_source=NULL;

static char *output_copyright;
static void (*write_object)(FILE *,section *,symbol *);
static int (*output_args)(char *);


void leave(void)
{
  section *sec;
  symbol *sym;
  
  if(outfile){
    fclose(outfile);
    if (errors)
      remove(outname);
  }

  if(debug){
    fprintf(stdout,"Sections:\n");
    for(sec=first_section;sec;sec=sec->next)
      print_section(stdout,sec);

    fprintf(stdout,"Symbols:\n");
    for(sym=first_symbol;sym;sym=sym->next){
      print_symbol(stdout,sym);
      fprintf(stdout,"\n");
    }
  }

  if(errors)
    exit(EXIT_FAILURE);
  else
    exit(EXIT_SUCCESS);
}

/* Removes all unallocated (offset) sections from the list and converts
   their label symbols into absolute expressions. */
static void remove_unalloc_sects(void)
{
  section *prev,*sec;
  symbol *sym;

  for (sym=first_symbol; sym; sym=sym->next) {
    if (sym->type==LABSYM && sym->sec!=NULL && (sym->sec->flags&UNALLOCATED)) {
      sym->type = EXPRESSION;
      sym->expr = number_expr(sym->pc);
      sym->sec = NULL;
    }
  }
  for (sec=first_section,prev=NULL; sec; sec=sec->next) {
    if (sec->flags&UNALLOCATED) {
      if (prev)
        prev->next = sec->next;
      else
        first_section = sec->next;
    }
    else
      prev = sec;
  }
}

/* append a new stabs (nlist) symbol/debugging definition */
static void new_stabdef(aoutnlist *nlist,section *sec)
{
  struct stabdef *new = mymalloc(sizeof(struct stabdef));

  new->next = NULL;
  new->name.ptr = nlist->name;
  new->type = nlist->type;
  new->other = nlist->other;
  new->desc = nlist->desc;
  new->base = NULL;
  if (nlist->value == NULL)
    new->value = 0;
  else if (!eval_expr(nlist->value,&new->value,sec,sec->pc)) {
    int btype = find_base(nlist->value,&new->base,sec,sec->pc);
    if (btype==BASE_ILLEGAL || btype==BASE_PCREL) {
       new->base = NULL;
       general_error(38);  /* illegal relocation */
    }
  }
  if (last_nlist)
    last_nlist = last_nlist->next = new;
  else
    first_nlist = last_nlist = new;
}


static void resolve_section(section *sec)
{
  taddr rorg_pc,org_pc;
  int fastphase=FASTOPTPHASE;
  int pass=0;
  int extrapass;
  size_t size;
  atom *p;

  do{
    done=1;
    rorg_pc=0;
    if (++pass>=MAXPASSES){
      general_error(7,sec->name);
      break;
    }
    extrapass=pass<=fastphase;
    if(debug)
      printf("resolve_section(%s) pass %d%s",sec->name,pass,
             pass<=fastphase?" (fast)\n":"\n");
    sec->pc=sec->org;
    for(p=sec->first;p;p=p->next){
      sec->pc=pcalign(p,sec->pc);
      cur_src=p->src;
      cur_src->line=p->line;
#if HAVE_CPU_OPTS
      if(p->type==OPTS){
        cpu_opts(p->content.opts);
      }
      else
#endif
      if(p->type==RORG){
        if(rorg_pc!=0)
          general_error(43);  /* reloc org is already set */
        rorg_pc=*p->content.rorg;
        org_pc=sec->pc;
        sec->pc=rorg_pc;
        sec->flags|=ABSOLUTE;
      }
      else if(p->type==RORGEND&&rorg_pc!=0){
        sec->pc=org_pc+(sec->pc-rorg_pc);
        rorg_pc=0;
        sec->flags&=~ABSOLUTE;
      }
      else if(p->type==LABEL){
        symbol *label=p->content.label;
        if(label->type!=LABSYM)
          ierror(0);
        if(label->pc!=sec->pc){
          if(debug)
            printf("moving label %s from %lu to %lu\n",label->name,
                   (unsigned long)label->pc,(unsigned long)sec->pc);
          done=0;
          label->pc=sec->pc;
        }
      }
      if(pass>fastphase&&!done&&p->type==INSTRUCTION){
        /* entered safe mode: optimize only one instruction every pass */
        sec->pc+=p->lastsize;
        continue;
      }
      if(p->changes>MAXSIZECHANGES){
        /* atom changed size too frequently, set warning flag */
        if(debug)
          printf("setting resolve-warning flag for atom type %d at %lu\n",
                 p->type,(unsigned long)sec->pc);
        sec->flags|=RESOLVE_WARN;
        size=atom_size(p,sec,sec->pc);
        sec->flags&=~RESOLVE_WARN;
      }
      else
        size=atom_size(p,sec,sec->pc);
      if(size!=p->lastsize){
        if(debug)
          printf("modify size of atom type %d at %lu from %lu to %lu\n",
                 p->type,(unsigned long)sec->pc,(unsigned long)p->lastsize,
                 (unsigned long)size);
        done=0;
        if(pass>fastphase)
          p->changes++;  /* now count size modifications of atoms */
        else if(size>p->lastsize)
          extrapass=0;   /* no extra pass, when an atom became larger */
        p->lastsize=size;
      }
      sec->pc+=size;
    }
    if(rorg_pc!=0){
      sec->pc=org_pc+(sec->pc-rorg_pc);
      sec->flags&=~ABSOLUTE;  /* workaround for misssing RORGEND */
    }
    /* Extend the fast-optimization phase, when there was no atom which
       became larger than in the previous pass. */
    if(extrapass) fastphase++;
  }while(errors==0&&!done);
}

static void resolve(void)
{
  section *sec;
  final_pass=0;
  if(debug)
    printf("resolve()\n");
  for(sec=first_section;sec;sec=sec->next)
    resolve_section(sec);
}

static void assemble(void)
{
  section *sec;
  taddr basepc;
  taddr rorg_pc=0;
  taddr org_pc;
  atom *p;
  char *attr;
  int bss;

  remove_unalloc_sects();
  final_pass=1;
  for(sec=first_section;sec;sec=sec->next){
    source *lasterrsrc=NULL;
    int lasterrline=0;
    sec->pc=sec->org;
    attr=sec->attr;
    bss=0;
    while(*attr){
      if(*attr++=='u'){
        bss=1;
        break;
      }
    }
    for(p=sec->first;p;p=p->next){
      basepc=sec->pc;
      sec->pc=pcalign(p,sec->pc);
      cur_src=p->src;
      cur_src->line=p->line;
      if(p->list&&p->list->atom==p){
        p->list->sec=sec;
        p->list->pc=sec->pc;
      }
      if(p->changes>MAXSIZECHANGES)
        sec->flags|=RESOLVE_WARN;
      /* print a warning on auto-aligned instructions or data */
      if(sec->pc!=basepc){
        atom *aa;
        if (p->type==LABEL&&p->next!=NULL&&p->next->line==p->line)
          aa=p->next; /* next atom in same line, look at it instead of label */
        else
          aa=p;
        if (aa->type==INSTRUCTION)
          general_error(50);  /* instruction has been auto-aligned */
        else if (aa->type==DATA||aa->type==DATADEF)
          general_error(57);  /* data has been auto-aligned */
      }
      if(p->type==RORG){
        rorg_pc=*p->content.rorg;
        org_pc=sec->pc;
        sec->pc=rorg_pc;
        sec->flags|=ABSOLUTE;
      }
      else if(p->type==RORGEND){
        if(rorg_pc!=0){
          sec->pc=org_pc+(sec->pc-rorg_pc);
          rorg_pc=0;
          sec->flags&=~ABSOLUTE;
        }
        else
          general_error(44);  /* reloc org was not set */
      }
      else if(p->type==INSTRUCTION){
        dblock *db;
        cur_listing=p->list;
        db=eval_instruction(p->content.inst,sec,sec->pc);
        if(pic_check)
          do_pic_check(db->relocs);
        cur_listing=0;
        if(debug){
          if(db->size!=(p->content.inst->code>=0?
                        instruction_size(p->content.inst,sec,sec->pc):0))
            ierror(0);
        }
        /*FIXME: sauber freigeben */
        myfree(p->content.inst);
        p->content.db=db;
        p->type=DATA;
      }
      else if(p->type==DATADEF){
        dblock *db;
        cur_listing=p->list;
        db=eval_data(p->content.defb->op,p->content.defb->bitsize,sec,sec->pc);
        if(pic_check)
          do_pic_check(db->relocs);
        cur_listing=0;
        /*FIXME: sauber freigeben */
        myfree(p->content.defb);
        p->content.db=db;
        p->type=DATA;
      }
      else if(p->type==ROFFS){
        sblock *sb;
        taddr space;
        if(eval_expr(p->content.roffs,&space,sec,sec->pc)){
          space=sec->org+space-sec->pc;
          if (space>=0){
            sb=new_sblock(number_expr(space),1,0);
            p->content.sb=sb;
            p->type=SPACE;
          }
          else
            general_error(20);  /* rorg is lower than current pc */
        }
        else
          general_error(30);  /* expression must be constant */
      }
      else if(p->type==DATA&&bss){
        if(lasterrsrc!=p->src||lasterrline!=p->line){
          general_error(31);  /* initialized data in bss */
          lasterrsrc=p->src;
          lasterrline=p->line;
        }
      }
#if HAVE_CPU_OPTS
      else if(p->type==OPTS)
        cpu_opts(p->content.opts);
#endif
      else if(p->type==PRINTTEXT&&!depend)
        printf("%s",p->content.ptext);
      else if(p->type==PRINTEXPR&&!depend)
        atom_printexpr(p->content.pexpr,sec,sec->pc);
      else if(p->type==ASSERT){
        assertion *ast=p->content.assert;
        taddr val;
        if(ast->assert_exp!=NULL) {
          eval_expr(ast->assert_exp,&val,sec,sec->pc);
          if(val==0)
            general_error(47,ast->expstr,ast->msgstr?ast->msgstr:emptystr);
        }
        else /* ASSERT without expression, used for user-FAIL directives */
          general_error(19,ast->msgstr?ast->msgstr:emptystr);
      }
      else if(p->type==NLIST)
        new_stabdef(p->content.nlist,sec);
      sec->pc+=atom_size(p,sec,sec->pc);
      sec->flags&=~RESOLVE_WARN;
    }
    /* leave RORG-mode, when section ends */
    if(rorg_pc!=0){
      sec->pc=org_pc+(sec->pc-rorg_pc);
      rorg_pc=0;
      sec->flags&=~ABSOLUTE;
    }
  }
}

static void undef_syms(void)
{
  symbol *sym;

  for(sym=first_symbol;sym;sym=sym->next){
    if (!auto_import&&sym->type==IMPORT&&!(sym->flags&(EXPORT|COMMON|WEAK)))
      general_error(22,sym->name);
    else if (sym->type==IMPORT&&!(sym->flags&REFERENCED))
      general_error(61,sym->name);
  }
}

static void fix_labels(void)
{
  symbol *sym,*base;
  taddr val;

  for(sym=first_symbol;sym;sym=sym->next){
    /* turn all absolute mode labels into absolute symbols */
    if((sym->flags&ABSLABEL)&&sym->type==LABSYM){
      sym->type=EXPRESSION;
      sym->flags&=~(TYPE_MASK|COMMON);
      sym->sec=NULL;
      sym->size=NULL;
      sym->align=0;
      sym->expr=number_expr(sym->pc);
    }
    /* expressions which are based on a label are turned into a new label */
    else if(sym->type==EXPRESSION){
      if(!eval_expr(sym->expr,&val,NULL,0)){
        if(find_base(sym->expr,&base,NULL,0)==BASE_OK){
          /* turn into an offseted label symbol from the base's section */
          sym->type=base->type;
          sym->sec=base->sec;
          sym->pc=val;
          sym->align=1;
        }else
          general_error(53,sym->name);  /* non-relocatable expr. in equate */
      }
    }
  }
}

static void statistics(void)
{
  section *sec;
  unsigned long long size;

  printf("\n");
  for(sec=first_section;sec;sec=sec->next){
    size=ULLTADDR(ULLTADDR(sec->pc)-ULLTADDR(sec->org));
    printf("%s(%s%lu):\t%12llu byte%c\n",sec->name,sec->attr,
           (unsigned long)sec->align,size,size==1?' ':'s');
  }
}

static int init_output(char *fmt)
{
  if(!strcmp(fmt,"test"))
    return init_output_test(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"elf"))
    return init_output_elf(&output_copyright,&write_object,&output_args);  
  if(!strcmp(fmt,"bin"))
    return init_output_bin(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"srec"))
    return init_output_srec(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"vobj"))
    return init_output_vobj(&output_copyright,&write_object,&output_args);  
  if(!strcmp(fmt,"hunk"))
    return init_output_hunk(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"aout"))
    return init_output_aout(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"hunkexe")){
    exec_out=1;  /* executable format */
    return init_output_hunkexe(&output_copyright,&write_object,&output_args);
  }
  if(!strcmp(fmt,"tos")){
    exec_out=1;  /* executable format */
    return init_output_tos(&output_copyright,&write_object,&output_args);
  }
  return 0;
}

static int init_main(void)
{
  size_t i;
  char *last;
  hashdata data;
  mnemohash=new_hashtable(MNEMOHTABSIZE);
  i=0;
  while(i<mnemonic_cnt){
    data.idx=i;
    last=mnemonics[i].name;
    add_hashentry(mnemohash,mnemonics[i].name,data);
    do{
      i++;
    }while(i<mnemonic_cnt&&!strcmp(last,mnemonics[i].name));
  }
  if(debug){
    if(mnemohash->collisions)
      printf("*** %d mnemonic collisions!!\n",mnemohash->collisions);
  }
  new_include_path(".");
  taddrmask=MAKEMASK(bytespertaddr<<3);
  return 1;
}

void set_default_output_format(char *fmt)
{
  output_format=fmt;
}

static void set_input_name(void)
{
  if(inname){
    char *p,c;
    if((p=strrchr(inname,'/'))!=NULL||
       (p=strrchr(inname,'\\'))!=NULL||
       (p=strrchr(inname,':'))!=NULL){
      /* source text not in current dir., add an include path for it */
      p++;
      c=*p;
      *p='\0';
      new_include_path(inname);
      *p=c;
    }
    setfilename(inname);
    setdebugname(inname);
    include_source(inname);
  }else
    general_error(15);
}

static void write_depends(FILE *f)
{
  struct deplist *d = first_depend;

  if (depend==DEPEND_MAKE && d!=NULL && outname!=NULL)
    fprintf(f,"%s:",outname);

  while (d != NULL) {
    switch (depend) {
      case DEPEND_LIST:
        fprintf(f,"%s\n",d->filename);
        break;
      case DEPEND_MAKE:
        if (str_is_graph(d->filename))
          fprintf(f," %s",d->filename);
        else
          fprintf(f," \"%s\"",d->filename);
        break;
      default:
        ierror(0);
    }
    d = d->next;
  }

  if (depend == DEPEND_MAKE)
    fputc('\n',f);
}

int main(int argc,char **argv)
{
  int i;
  for(i=1;i<argc;i++){
    if(argv[i][0]=='-'&&argv[i][1]=='F'){
      output_format=argv[i]+2;
      argv[i][0]=0;
    }
    if(!strcmp("-quiet",argv[i])){
      verbose=0;
      argv[i][0]=0;
    }
    if(!strcmp("-debug",argv[i])){
      debug=1;
      argv[i][0]=0;
    }
  }
  if(!init_output(output_format))
    general_error(16,output_format);
  if(!init_main())
    general_error(10,"main");
  if(!init_symbol())
    general_error(10,"symbol");
  if(verbose)
    printf("%s\n%s\n%s\n%s\n",copyright,cpu_copyright,syntax_copyright,output_copyright);
  for(i=1;i<argc;i++){
    if(argv[i][0]==0)
      continue;
    if(argv[i][0]!='-'){
      if(inname)
        general_error(11);
      inname=argv[i];
      continue;
    }
    if(!strcmp("-o",argv[i])&&i<argc-1){
      if(outname)
        general_error(28,'o');
      outname=argv[++i];
      continue;
    }
    if(!strcmp("-L",argv[i])&&i<argc-1){
      if(listname)
        general_error(28,'L');
      listname=argv[++i];
      produce_listing=1;
      set_listing(1);
      continue;
    }
    if(!strcmp("-Lnf",argv[i])){
      listformfeed=0;
      continue;
    }
    if(!strcmp("-Lns",argv[i])){
      listnosyms=1;
      continue;
    }
    if(!strncmp("-Ll",argv[i],3)){
      sscanf(argv[i]+3,"%i",&listlinesperpage);
      continue;
    }
    if(!strncmp("-D",argv[i],2)){
      char *def=NULL;
      expr *val;
      if(argv[i][2])
        def=&argv[i][2];
      else if (i<argc-1)
        def=argv[++i];
      if(def){
        char *s=def;
        if(ISIDSTART(*s)){
          s++;
          while(ISIDCHAR(*s))
            s++;
          def=cnvstr(def,s-def);
          if(*s=='='){
            s++;
            val=parse_expr(&s);
          }
          else
            val=number_expr(1);
          if(*s)
            general_error(23,'D');  /* trailing garbage after option */
          new_abs(def,val);
          myfree(def);
          continue;
        }
      }
    }
    if(!strncmp("-I",argv[i],2)){
      char *path=NULL;
      if(argv[i][2])
        path=&argv[i][2];
      else if (i<argc-1)
        path=argv[++i];
      if(path){
        new_include_path(path);
        continue;
      }
    }
    if(!strncmp("-depend=",argv[i],8)){
      if (!strcmp("list",&argv[i][8])) {
        depend=DEPEND_LIST;
        continue;
      }
      else if (!strcmp("make",&argv[i][8])) {
        depend=DEPEND_MAKE;
        continue;
      }
    }
    if(!strcmp("-unnamed-sections",argv[i])){
      unnamed_sections=1;
      continue;
    }
    if(!strcmp("-ignore-mult-inc",argv[i])){
      ignore_multinc=1;
      continue;
    }
    if(!strcmp("-nocase",argv[i])){
      nocase=1;
      continue;
    }
    if(!strcmp("-nosym",argv[i])){
      no_symbols=1;
      continue;
    }
    if(!strncmp("-nowarn=",argv[i],8)){
      int wno;
      sscanf(argv[i]+8,"%i",&wno);
      disable_warning(wno);
      continue;
    }
    else if(!strcmp("-w",argv[i])){
      no_warn=1;
      continue;
    }
    if(!strncmp("-maxerrors=",argv[i],11)){
      sscanf(argv[i]+11,"%i",&max_errors);
      continue;
    }
    else if(!strcmp("-pic",argv[i])){
      pic_check=1;
      continue;
    }
    else if(!strncmp("-maxmacrecurs=",argv[i],14)) {
      sscanf(argv[i]+14,"%i",&maxmacrecurs);
      continue;
    }
    else if(!strcmp("-unsshift",argv[i])){
      unsigned_shift=1;
      continue;
    }
    else if(!strcmp("-chklabels",argv[i])){
      chklabels=1;
      continue;
    }
    if(cpu_args(argv[i]))
      continue;
    if(syntax_args(argv[i]))
      continue;
    if(output_args(argv[i]))
      continue;
    if(!strcmp("-esc",argv[i])){
      esc_sequences=1;
      continue;
    }
    if(!strcmp("-noesc",argv[i])){
      esc_sequences=0;
      continue;
    }
    if (!strncmp("-x",argv[i],2)){
      auto_import=0;
      continue;
    }
    general_error(14,argv[i]);
  }
  set_input_name();
  internal_abs(vasmsym_name);
  if(!init_parse())
    general_error(10,"parse");
  if(!init_syntax())
    general_error(10,"syntax");
  if(!init_cpu())
    general_error(10,"cpu");
  parse();
  if(errors==0||produce_listing)
    resolve();
  if(errors==0||produce_listing)
    assemble();
  cur_src=NULL;
  if(errors==0)
    undef_syms();
  fix_labels();
  if(produce_listing){
    if(!listname)
      listname="a.lst";
    write_listing(listname);
  }
  if(errors==0){
    if(!depend){
      if(verbose)
        statistics();
      if(!outname)
        outname="a.out";
      outfile=fopen(outname,"wb");
      if(!outfile)
        general_error(13,outname);
      else
        write_object(outfile,first_section,first_symbol);
    }else
      write_depends(stdout);
  }
  leave();
  return 0; /* not reached */
}

static void add_depend(char *name)
{
  if (depend) {
    struct deplist *d = first_depend;

    /* check if an entry with the same file name already exists */
    while (d != NULL) {
      if (!strcmp(d->filename,name))
        return;
      d = d->next;
    }

    /* append new dependency record */
    d = mymalloc(sizeof(struct deplist));
    d->next = NULL;
    if (name[0]=='.'&&(name[1]=='/'||name[1]=='\\'))
      name += 2;  /* skip "./" in paths */
    d->filename = mystrdup(name);
    if (last_depend)
      last_depend = last_depend->next = d;
    else
      first_depend = last_depend = d;
  }
}

FILE *locate_file(char *filename,char *mode)
{
  char pathbuf[MAXPATHLEN];
  struct include_path *ipath;
  FILE *f;

  if (*filename=='.' || *filename=='/' || *filename=='\\' ||
      strchr(filename,':')!=NULL) {
    /* file name is absolute, then don't use any include paths */
    if (f = fopen(filename,mode)) {
      add_depend(filename);
      return f;
    }
  }
  else {
    /* locate file name in all known include paths */
    for (ipath=first_incpath; ipath; ipath=ipath->next) {
      if (strlen(ipath->path) + strlen(filename) + 1 <= MAXPATHLEN) {
        strcpy(pathbuf,ipath->path);
        strcat(pathbuf,filename);
        if (f = fopen(pathbuf,mode)) {
          add_depend(pathbuf);
          return f;
        }
      }
    }
  }
  general_error(12,filename);
  return NULL;
}

void include_source(char *inname)
{
  char *filename;
  struct include_path **nptr = &first_source;
  struct include_path *name;
  FILE *f;

  filename = convert_path(inname);

  /* check whether this source was already included */
  while (name = *nptr) {
#if defined(AMIGA) || defined(MSDOS) || defined(ATARI) || defined(_WIN32)
    if (!stricmp(name->path,filename)) {
#else
    if (!strcmp(name->path,filename)) {
#endif
      myfree(filename);
      if (!ignore_multinc) {
        filename = name->path;
        /* reuse already read file from cache? */
      }
      nptr = NULL;  /* ignore including this source */
      break;
    }
    nptr = &name->next;
  }
  if (nptr) {
    name = mymalloc(sizeof(struct include_path));
    name->next = NULL;
    name->path = filename;
    *nptr = name;
  }
  else if (ignore_multinc)
    return;  /* ignore multiple inclusion of this source completely */

  if (f = locate_file(filename,"r")) {
    char *text;
    size_t size;

    for (text=NULL,size=0; ; size+=SRCREADINC) {
      size_t nchar;
      text = myrealloc(text,size+SRCREADINC);
      nchar = fread(text+size,1,SRCREADINC,f);
      if (nchar < SRCREADINC) {
        size += nchar;
        break;
      }
    }
    if (feof(f)) {
      if (size > 0) {
        cur_src = new_source(filename,myrealloc(text,size+2),size+1);
        *(cur_src->text+size) = '\n';
        *(cur_src->text+size+1) = '\0';
      }
      else {
        myfree(text);
        cur_src = new_source(filename,"\n",1);
      }
    }
    else
      general_error(29,filename);
    fclose(f);
  }
}

/* searches a section by name and attr (if secname_attr set) */
section *find_section(char *name,char *attr)
{
  section *p;
  if(secname_attr){
    for(p=first_section;p;p=p->next){
      if(!strcmp(name,p->name) && !strcmp(attr,p->attr))
        return p;
    }
  }
  else{
    for(p=first_section;p;p=p->next){
      if(!strcmp(name,p->name))
        return p;
    }
  }
  return 0;
}

/* create a new source text instance, which has cur_src as parent */
source *new_source(char *filename,char *text,size_t size)
{
  static unsigned long id = 0;
  source *s = mymalloc(sizeof(source));
  char *p;
  size_t i;

  /* scan source for strange characters */
  for (p=text,i=0; i<size; i++,p++) {
    if (*p == 0x1a) {
      /* EOF character - replace by newline and ignore rest of source */
      *p = '\n';
      size = i + 1;
      break;
    }
  }

  s->parent = cur_src;
  s->parent_line = cur_src ? cur_src->line : 0;
  s->name = mystrdup(filename);
  s->text = text;
  s->size = size;
  s->macro = NULL;
  s->repeat = 1;        /* read just once */
  s->cond_level = clev; /* remember level of conditional nesting */
  s->num_params = -1;   /* not a macro, no parameters */
  s->param[0] = emptystr;
  s->param_len[0] = 0;
  s->id = id++;	        /* every source has unique id - important for macros */
  s->srcptr = text;
  s->line = 0;
  s->linebuf = mymalloc(MAXLINELENGTH);
#ifdef CARGSYM
  s->cargexp = NULL;
#endif
#ifdef REPTNSYM
  /* -1 outside of a repetition block */
  s->reptn = cur_src ? cur_src->reptn : -1;
#endif
  return s;
}

/* quit parsing the current source instance, leave macros, repeat loops
   and restore the conditional assembly level */
void end_source(source *s)
{
  if(s){
    s->srcptr=s->text+s->size;
    s->repeat=1;
    clev=s->cond_level;
  }
}

/* set current section, remember last */
void set_section(section *s)
{
#if NOT_NEEDED
  if (current_section!=NULL && !(current_section->flags & UNALLOCATED)) {
    if (current_section->flags & ABSOLUTE)
      prev_org = current_section;
    else
      prev_sec = current_section;
  }
#endif
#if HAVE_CPU_OPTS
  if (!(s->flags & UNALLOCATED))
    cpu_opts_init(s);  /* set initial cpu opts before the first atom */
#endif
  current_section = s;
}

/* creates a new section with given attributes and alignment;
   does not switch to this section automatically */
section *new_section(char *name,char *attr,int align)
{
  section *p;
  if(unnamed_sections)
    name=emptystr;
  if(p=find_section(name,attr))
    return p;
  p=mymalloc(sizeof(*p));
  p->next=0;
  p->name=mystrdup(name);
  p->attr=mystrdup(attr);
  p->first=p->last=0;
  p->align=align;
  p->org=p->pc=0;
  p->flags=0;
  p->memattr=0;
  memset(p->pad,0,MAXPADBYTES);
  p->padbytes=1;
  if(last_section)
    last_section=last_section->next=p;
  else
    first_section=last_section=p;
  return p;
}

/* create a dummy code section for each new ORG directive */
section *new_org(taddr org)
{
  char buf[16];
  section *sec;

  sprintf(buf,"seg%llx",ULLTADDR(org));
  sec = new_section(buf,"acrwx",1);
  sec->org = sec->pc = org;
  sec->flags |= ABSOLUTE;  /* absolute destination address */
  return sec;
}

/* switches current section to the section with the specified name */
void switch_section(char *name,char *attr)
{
  section *p;
  if(unnamed_sections)
    name=emptystr;
  p=find_section(name,attr);
  if(!p)
    general_error(2,name);
  else
    set_section(p);
}

/* Switches current section to an offset section. Create a new section when
   it doesn't exist yet or needs a different offset. */
void switch_offset_section(char *name,taddr offs)
{
  static unsigned long id;
  char unique_name[14];
  section *sec;

  if (!name) {
    if (offs != -1)
      ++id;
    sprintf(unique_name,"OFFSET%06lu",id);
    name = unique_name;
  }
  sec = new_section(name,"u",1);
  sec->flags |= UNALLOCATED;
  if (offs != -1)
    sec->org = sec->pc = offs;
  set_section(sec);
}

/* returns current_section or the syntax module's default section,
   when undefined */
section *default_section(void)
{
  section *sec = current_section;

  if (!sec && defsectname && defsecttype) {
    sec = new_section(defsectname,defsecttype,1);
    switch_section(defsectname,defsecttype);
  }
  return sec;
}

#if NOT_NEEDED
/* restore last relocatable section */
section *restore_section(void)
{
  if (prev_sec)
    return prev_sec;
  if (defsectname && defsecttype)
    return new_section(defsectname,defsecttype,1);
  return NULL;  /* no previous section or default section defined */
}

/* restore last absolute section */
section *restore_org(void)
{
  if (prev_org)
    return prev_org;
  return new_org(0);  /* no previous org: default to ORG 0 */
}
#endif /* NOT_NEEDED */

/* end a relocated ORG block */
int end_rorg(void)
{
  section *s = default_section();

  if (s == NULL) {
    general_error(3);
    return 0;
  }
  if (s->flags & IN_RORG) {
    add_atom(s,new_rorgend_atom());
    if (s->flags & PREVABS)
      s->flags |= ABSOLUTE;
    else
      s->flags &= ~ABSOLUTE;
    s->flags &= ~IN_RORG;
    return 1;
  }
  general_error(44);  /* no Rorg block to end */
  return 0;
}

/* end a relocated ORG block when currently active */
void try_end_rorg(void)
{
  if (current_section!=NULL && (current_section->flags&IN_RORG))
    end_rorg();
}

/* start a relocated ORG block */
void start_rorg(taddr rorg)
{
  section *s = default_section();

  if (s == NULL) {
    general_error(3);
    return;
  }
  if (s->flags & IN_RORG)
    end_rorg();  /* we are already in a ROrg-block, so close it first */
  add_atom(s,new_rorg_atom(rorg));
  s->flags |= IN_RORG;
  if (!(s->flags & ABSOLUTE)) {
    s->flags &= ~PREVABS;
    s->flags |= ABSOLUTE;  /* make section absolute during the ROrg-block */
  }
  else
    s->flags |= PREVABS;
}

void print_section(FILE *f,section *sec)
{
  atom *p;
  taddr pc=sec->org;
  fprintf(f,"section %s (attr=<%s> align=%llu):\n",
          sec->name,sec->attr,ULLTADDR(sec->align));
  for(p=sec->first;p;p=p->next){
    pc=pcalign(p,pc);
    fprintf(f,"%8llx: ",ULLTADDR(pc));
    print_atom(f,p);
    fprintf(f,"\n");
    pc+=atom_size(p,sec,pc);
  }
}

void new_include_path(char *pathname)
{
  struct include_path *new = mymalloc(sizeof(struct include_path));
  struct include_path *ipath;
  char *newpath = convert_path(pathname);
  int len = strlen(newpath);

#if defined(AMIGA)
  if (len>0 && newpath[len-1]!='/' && newpath[len-1]!=':') {
    pathname = mymalloc(len+2);
    strcpy(pathname,newpath);
    pathname[len] = '/';
    pathname[len+1] = '\0';
  }
#elif defined(MSDOS) || defined(ATARI) || defined(_WIN32)
  if (len>0 && newpath[len-1]!='\\' && newpath[len-1]!=':') {
    pathname = mymalloc(len+2);
    strcpy(pathname,newpath);
    pathname[len] = '\\';
    pathname[len+1] = '\0';
  }
#else
  if (len>0 && newpath[len-1] != '/') {
    pathname = mymalloc(len+2);
    strcpy(pathname,newpath);
    pathname[len] = '/';
    pathname[len+1] = '\0';
  }
#endif
  else
    pathname = mystrdup(newpath);
  myfree(newpath);
  new->next = NULL;
  new->path = pathname;

  if (ipath = first_incpath) {
    while (ipath->next)
      ipath = ipath->next;
    ipath->next = new;
  }
  else
    first_incpath = new;
}

void set_listing(int on)
{
  listena = on && produce_listing;
}

void set_list_title(char *p,int len)
{
  listtitlecnt++;
  listtitles=myrealloc(listtitles,listtitlecnt*sizeof(*listtitles));
  listtitles[listtitlecnt-1]=mymalloc(len+1);
  strncpy(listtitles[listtitlecnt-1],p,len);
  listtitles[listtitlecnt-1][len]=0;
  listtitlelines=myrealloc(listtitlelines,listtitlecnt*sizeof(*listtitlelines));
  listtitlelines[listtitlecnt-1]=cur_src->line;
}

static void print_list_header(FILE *f,int cnt)
{
  if(cnt%listlinesperpage==0){
    if(cnt!=0&&listformfeed)
      fprintf(f,"\f");
    if(listtitlecnt>0){
      int i,t;
      for(i=0,t=-1;i<listtitlecnt;i++){
        if(listtitlelines[i]<=cnt+listlinesperpage)
          t=i;
      }
      if(t>=0){
        int sp=(120-strlen(listtitles[t]))/2;
        while(--sp)
          fprintf(f," ");
        fprintf(f,"%s\n",listtitles[t]);
      }
      cnt++;
    }
    fprintf(f,"Err  Line Loc.  S Object1  Object2  M Source\n");
  }  
}

#if VASM_CPU_OIL
void write_listing(char *listname)
{
  FILE *f;
  int nsecs,i,cnt=0,nl;
  section *secp;
  listing *p;
  atom *a;
  symbol *sym;
  taddr pc;
  char rel;

  if(!(f=fopen(listname,"w"))){
    general_error(13,listname);
    return;
  }
  for(nsecs=0,secp=first_section;secp;secp=secp->next)
    secp->idx=nsecs++;
  for(p=first_listing;p;p=p->next){
    if(!p->src||p->src->id!=0)
      continue;
    print_list_header(f,cnt++);
    if(p->error!=0)
      fprintf(f,"%04d ",p->error);
    else
      fprintf(f,"     ");
    fprintf(f,"%4d ",p->line);
    a=p->atom;
    while(a&&a->type!=DATA&&a->next&&a->next->line==a->line&&a->next->src==a->src)
      a=a->next;
    if(a&&a->type==DATA){
      int size=a->content.db->size;
      char *dp=a->content.db->data;
      pc=p->pc;
      fprintf(f,"%05lX %d ",(unsigned long)pc,(int)(p->sec?p->sec->idx:0));
      for(i=0;i<8;i++){
        if(i==4)
          fprintf(f," ");
        if(i<size){
          fprintf(f,"%02X",(unsigned char)*dp++);
          pc++;
        }else
          fprintf(f,"  ");
        /* append following atoms with align 1 directly */
        if(i==size-1&&i<7&&a->next&&a->next->align<=a->align&&a->next->type==DATA&&a->next->line==a->line&&a->next->src==a->src){
          a=a->next;
          size+=a->content.db->size;
          dp=a->content.db->data;
        }
      }
      fprintf(f," ");
      if(a->content.db->relocs){
        symbol *s=((nreloc *)(a->content.db->relocs->reloc))->sym;
        if(s->type==IMPORT)
          rel='X';
        else
          rel='0'+p->sec->idx;
      }else
        rel='A';
      fprintf(f,"%c ",rel);
    }else
      fprintf(f,"                           ");
    
    fprintf(f," %-.77s",p->txt);

    /* bei laengeren Daten den Rest ueberspringen */
    /* Block entfernen, wenn alles ausgegeben werden soll */
    if(a&&a->type==DATA&&i<a->content.db->size){
      pc+=a->content.db->size-i;
      i=a->content.db->size;
    }

    /* restliche DATA-Zeilen, wenn noetig */
    while(a){
      if(a->type==DATA){
        int size=a->content.db->size;
        char *dp=a->content.db->data+i;

        if(i<size){
          for(;i<size;i++){
            if((i&7)==0){
              fprintf(f,"\n");
              print_list_header(f,cnt++);
              fprintf(f,"          %05lX %d ",(unsigned long)pc,(int)(p->sec?p->sec->idx:0));
            }else if((i&3)==0)
              fprintf(f," ");
            fprintf(f,"%02X",(unsigned char)*dp++);
            pc++;
            /* append following atoms with align 1 directly */
            if(i==size-1&&a->next&&a->next->align<=a->align&&a->next->type==DATA&&a->next->line==a->line&&a->next->src==a->src){
              a=a->next;
              size+=a->content.db->size;
              dp=a->content.db->data;
            }
          }
          i=8-(i&7);
          if(i>=4)
            fprintf(f," ");
          while(i--){
            fprintf(f,"  ");
          }
          fprintf(f," %c",rel);
        }
        i=0;
      }
      if(a->next&&a->next->line==a->line&&a->next->src==a->src){
        a=a->next;
        pc=pcalign(a,pc);
        if(a->type==DATA&&a->content.db->relocs){
          symbol *s=((nreloc *)(a->content.db->relocs->reloc))->sym;
          if(s->type==IMPORT)
            rel='X';
          else
            rel='0'+p->sec->idx;
        }else
          rel='A';      
      }else
        a=0;
    }
    fprintf(f,"\n");
  }
  fprintf(f,"\n\nSections:\n");
  for(secp=first_section;secp;secp=secp->next)
    fprintf(f,"%d  %s\n",(int)secp->idx,secp->name);
  if(!listnosyms){
    fprintf(f,"\n\nSymbols:\n");
    {
      symbol *last=0,*cur,*symo;
      for(symo=first_symbol;symo;symo=symo->next){
        cur=0;
        for(sym=first_symbol;sym;sym=sym->next){
          if(!last||stricmp(sym->name,last->name)>0)
            if(!cur||stricmp(sym->name,cur->name)<0)
              cur=sym;
        }
        if(cur){
          print_symbol(f,cur);
          fprintf(f,"\n");
          last=cur;
        }
      }
    }
  }
  if(errors==0)
    fprintf(f,"\nThere have been no errors.\n");
  else
    fprintf(f,"\nThere have been %d errors!\n",errors);
  fclose(f);
  for(p=first_listing;p;){
    listing *m=p->next;
    myfree(p);
    p=m;
  }
}
#else
void write_listing(char *listname)
{
  FILE *f;
  int nsecs,i,maxsrc=0;
  section *secp;
  listing *p;
  atom *a;
  symbol *sym;
  taddr pc;

  if(!(f=fopen(listname,"w"))){
    general_error(13,listname);
    return;
  }
  for(nsecs=1,secp=first_section;secp;secp=secp->next)
    secp->idx=nsecs++;
  for(p=first_listing;p;p=p->next){
    char err[6];
    if(p->error!=0)
      sprintf(err,"E%04d",p->error);
    else
      sprintf(err,"     ");
    if(p->src&&p->src->id>maxsrc)
      maxsrc=p->src->id;
    fprintf(f,"F%02d:%04d %s %s",(int)(p->src?p->src->id:0),p->line,err,p->txt);
    a=p->atom;
    pc=p->pc;
    while(a){
      if(a->type==DATA){
        int size=a->content.db->size;
        for(i=0;i<size&&i<32;i++){
          if((i&15)==0)
            fprintf(f,"\n               S%02d:%08lX: ",(int)(p->sec?p->sec->idx:0),(unsigned long)(pc));
          fprintf(f," %02X",(unsigned char)a->content.db->data[i]);
          pc++;
        }
        if(a->content.db->relocs)
          fprintf(f," [R]");
      }
      if(a->next&&a->next->line==a->line&&a->next->src==a->src){
        a=a->next;
        pc=pcalign(a,pc);
      }else
        a=0;
    }
    fprintf(f,"\n");
  }
  fprintf(f,"\n\nSections:\n");
  for(secp=first_section;secp;secp=secp->next)
    fprintf(f,"S%02d  %s\n",(int)secp->idx,secp->name);
  fprintf(f,"\n\nSources:\n");
  for(i=0;i<=maxsrc;i++){
    for(p=first_listing;p;p=p->next){
      if(p->src&&p->src->id==i){
        fprintf(f,"F%02d  %s\n",i,p->src->name);
        break;
      }
    }
  }
  fprintf(f,"\n\nSymbols:\n");
  for(sym=first_symbol;sym;sym=sym->next){
    print_symbol(f,sym);
    fprintf(f,"\n");
  }
  if(errors==0)
    fprintf(f,"\nThere have been no errors.\n");
  else
    fprintf(f,"\nThere have been %d errors!\n",errors);
  fclose(f);
  for(p=first_listing;p;){
    listing *m=p->next;
    myfree(p);
    p=m;
  }
}
#endif
