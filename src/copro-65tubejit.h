// copro-65tube.h
#ifndef COPRO_65TUBEJIT_H
#define COPRO_65TUBEJIT_H

#define JITLET 0x0d000000
#define JITTEDTABLE16 0x0d100000

extern void copro_65tubejit_emulator();

extern void exec_65tubejit(unsigned char *memory, unsigned int speed);

#endif
