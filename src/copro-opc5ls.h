// copro-opc5ls.h
#ifndef COPRO_OPC5LS_H
#define COPRO_OPC5LS_H

// #define DBG

#ifdef DBG
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

extern void copro_opc5ls_write(uint16_t addr, uint16_t data);
extern uint16_t copro_opc5ls_read(uint16_t addr);
extern void copro_opc5ls_emulator();

#endif
