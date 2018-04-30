// copro-pdp11.h
#ifndef COPRO_PDP11_H
#define COPRO_PDP11_H

extern void copro_pdp11_write8(uint16_t addr, uint8_t data);

extern uint8_t copro_pdp11_read8(uint16_t addr);

extern void copro_pdp11_write16(uint16_t addr, uint16_t data);

extern uint16_t copro_pdp11_read16(uint16_t addr);

extern void copro_pdp11_emulator();

#endif
