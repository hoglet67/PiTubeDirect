// copro-80186.h
#ifndef COPRO_80186_H
#define COPRO_80186_H

extern void copro_80186_emulator();

extern unsigned int copro_80186_tube_read(uint16_t addr);

extern void copro_80186_tube_write(uint16_t addr, uint8_t data);

extern void copro_80186_write_hook(uint32_t addr32, uint8_t value);

#endif
