#ifndef COPRO_F100_H
#define COPRO_F100_H

// #define DBG

#ifdef DBG
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

extern void copro_f100_write_mem(uint16_t addr, uint16_t data);
extern uint16_t copro_f100_read_mem(uint16_t addr);
extern void copro_f100_emulator();

#endif
