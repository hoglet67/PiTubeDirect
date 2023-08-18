// copro-riscv.h
#ifndef COPRO_RISCV_H
#define COPRO_RISCV_H

#define RESET_ADDRESS 0x00FC0000

extern void copro_riscv_write_mem32(uint32_t addr, uint32_t data);
extern void copro_riscv_write_mem16(uint32_t addr, uint32_t data);
extern void copro_riscv_write_mem8(uint32_t addr, uint32_t data);

extern uint32_t copro_riscv_read_mem32(uint32_t addr);
extern uint32_t copro_riscv_read_mem16(uint32_t addr);
extern uint32_t copro_riscv_read_mem16_signed(uint32_t addr);
extern uint32_t copro_riscv_read_mem8(uint32_t addr);
extern uint32_t copro_riscv_read_mem8_signed(uint32_t addr);

extern void copro_riscv_emulator();

#define MINIRV32_DECORATE extern

#include "riscv/mini-rv32ima.h"

extern struct MiniRV32IMAState *riscv_state;

#endif
