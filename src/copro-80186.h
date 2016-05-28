// copro-80186.h
#ifndef COPRO_80186_H
#define COPRO_80186_H

extern void copro_80186_emulator();

extern int copro_80186_tube_read(uint16_t addr);

extern void copro_80186_tube_write(uint16_t addr, uint8_t data);

#endif
