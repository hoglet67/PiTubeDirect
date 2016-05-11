// tube-ula.h

#ifndef TUBE_ULA_H
#define TUBE_ULA_H

extern uint8_t tube_host_read(uint16_t addr);

extern void tube_host_write(uint16_t addr, uint8_t val);

extern uint8_t tube_parasite_read(uint32_t addr);

extern void tube_parasite_write(uint32_t addr, uint8_t val);

extern void tube_reset();

extern int tube_io_handler(uint32_t mail);

extern int tube_init_hardware();

extern int tube_is_rst_active();

extern void tube_wait_for_rst_active();

extern void tube_wait_for_rst_release();

#endif
