/* supp.h miscellaneous support routines */
/* (c) in 2008-2017 by Frank Wille */

#ifndef SUPP_H
#define SUPP_H

struct node {
  struct node *next;
  struct node *pred;
};

struct list {
  struct node *first;
  struct node *dummy;
  struct node *last;
};

void initlist(struct list *);
void addtail(struct list *,struct node *);
struct node *remnode(struct node *);
struct node *remhead(struct list *);

void *mymalloc(size_t);
void *mycalloc(size_t);
void *myrealloc(void *,size_t);
void myfree(void *);

int field_overflow(int,size_t,taddr);
uint64_t readval(int,void *,size_t);
void *setval(int,void *,size_t,uint64_t);
void *setval_signext(int,void *,size_t,size_t,int64_t);
uint64_t readbits(int,void *,unsigned,unsigned,unsigned);
void setbits(int,void *,unsigned,unsigned,unsigned,uint64_t);
int countbits(taddr);
void copy_cpu_taddr(void *,taddr,size_t);

void conv2ieee32(int,uint8_t *,tfloat);
void conv2ieee64(int,uint8_t *,tfloat);
void conv2ieee80(int,uint8_t *,tfloat);
void conv2ieee128(int,uint8_t *,tfloat);
int flt_chkrange(tfloat,int);

void fw8(FILE *,uint8_t);
void fw16(FILE *,uint16_t,int);
void fw32(FILE *,uint32_t,int);
void fwdata(FILE *,void *,size_t);
void fwsblock(FILE *,sblock *);
void fwspace(FILE *,size_t);
void fwalign(FILE *,taddr,taddr);
taddr fwpcalign(FILE *,atom *,section *,taddr);
size_t filesize(FILE *);
int abs_path(char *);
char *convert_path(char *);

int stricmp(const char *,const char *);
int strnicmp(const char *,const char *,size_t);
char *mystrdup(char *);
char *cnvstr(char *,int);
char *strtolower(char *);
int str_is_graph(const char *);
const char *trim(const char *);

taddr balign(taddr,taddr);
taddr palign(taddr,taddr);
taddr pcalign(atom *,taddr);

taddr get_sym_value(symbol *);
taddr get_sym_size(symbol *);
utaddr get_sec_size(section *);

#endif
