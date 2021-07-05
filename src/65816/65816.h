#ifndef __INC_65816_H
#define __INC_65816_H

#ifdef INCLUDE_DEBUGGER
#include "../cpu_debug.h"
#endif

enum register_numbers {
    REG_A,
    REG_X,
    REG_Y,
    REG_S,
    REG_P,
    REG_PC,
    REG_DP,
    REG_DB,
    REG_PB
};

typedef struct
{
    int c,z,i,d,b,v,n,m,ex,e; /*X renamed to EX due to #define conflict*/
} w65816p_t;

extern w65816p_t w65816p;

void w65816_init(void *rom, uint32_t nativeVectBank);
void w65816_reset(void);
void w65816_exec(int tubecycles);
void w65816_close(void);

#endif
