// copro-65tube.h
#ifndef COPRO_65TUBEJIT_H
#define COPRO_65TUBEJIT_H

#define JITLET 0x0c000000
#define JITTEDTABLE16 0x0C100000

#if !defined(__ASSEMBLER__)
extern void copro_65tubejit_emulator();

extern void exec_65tubejit(unsigned char *memory, unsigned int speed);

extern uint32_t * jit_get_regs(void);
#endif

#endif
