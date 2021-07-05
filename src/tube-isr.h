// tube-isr.h

#ifndef TUBE_ISR_H
#define TUBE_ISR_H

#include "copro-armnative.h"

extern volatile unsigned char *tube_address;

#ifdef TUBE_ISR_STATE_MACHINE
extern void copro_armnative_tube_interrupt_handler(uint32_t mail);
#else
extern void copro_armnative_tube_interrupt_handler();
#endif

void set_ignore_transfer(int on);

#endif
