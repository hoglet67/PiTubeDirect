// tube-ula.h

#ifndef TUBE_ULA_H
#define TUBE_ULA_H

extern uint8_t copro_65tube_host_read(uint16_t addr);

extern void copro_65tube_host_write(uint16_t addr, uint8_t val);

extern uint8_t copro_65tube_tube_read(uint32_t addr);

extern void copro_65tube_tube_write(uint32_t addr, uint8_t val);

extern void copro_65tube_tube_reset();

#endif
