/* -*- mode: c; c-basic-offset: 4 -*- */

#ifndef __INCLUDE_B_EM_65816DEBUG__
#define __INCLUDE_B_EM_65816DEBUG__

#include "../cpu_debug.h"

extern const cpu_debug_t w65816_cpu_debug;

uint32_t dbg65816_disassemble(const cpu_debug_t *cpu, uint32_t addr, char *buf, size_t bufsize);

#endif
