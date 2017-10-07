// copro-opc6.h
#ifndef COPRO_OPC6_H
#define COPRO_OPC6_H

// #define DBG

#ifdef DBG
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

extern void copro_opc6_write_mem(uint16_t addr, uint16_t data);
extern uint16_t copro_opc6_read_mem(uint16_t addr);
extern void copro_opc6_write_io(uint16_t addr, uint16_t data);
extern uint16_t copro_opc6_read_io(uint16_t addr);
extern void copro_opc6_emulator();

#endif
