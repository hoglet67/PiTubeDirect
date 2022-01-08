#ifndef __INC_65816_H
#define __INC_65816_H

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

uint8_t *w65816_init(const void *rom, uint32_t nativeVectBank);
void w65816_reset(void);
void w65816_exec(int tubecycles);
void w65816_close(void);

#endif
