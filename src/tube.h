// tube.h

#ifndef TUBE_H
#define TUBE_H

#include <inttypes.h>

extern volatile int copro;

extern volatile uint32_t tube_mailbox;

extern void arm_irq_handler();

extern void arm_fiq_handler_flag0();

extern void arm_fiq_handler_flag1();

extern void lock_isr();

#endif
