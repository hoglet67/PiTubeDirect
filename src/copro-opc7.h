// copro-opc7.h
#ifndef COPRO_OPC7_H
#define COPRO_OPC7_H

// #define DBG

#ifdef DBG
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

extern void copro_opc7_write_mem(uint32_t addr, uint32_t data);
extern uint32_t copro_opc7_read_mem(uint32_t addr);
extern void copro_opc7_write_io(uint32_t addr, uint32_t data);
extern uint32_t copro_opc7_read_io(uint32_t addr);
extern void copro_opc7_emulator();

#endif
