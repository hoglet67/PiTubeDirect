// tube-isr.h

#ifndef TUBE_ISR_H
#define TUBE_ISR_H

// If this is defined, the tube interrupt handler is implemented as a state machine
// Otherwise, it is implemented as code that may block
#define TUBE_ISR_STATE_MACHINE

extern volatile unsigned char *address;

#ifdef TUBE_ISR_STATE_MACHINE
extern void copro_armnative_tube_interrupt_handler(uint32_t mail);
#else
extern void copro_armnative_tube_interrupt_handler();
#endif

#endif
