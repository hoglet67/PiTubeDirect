// tube-isr.h

#ifndef TUBE_ISR_H
#define TUBE_ISR_H

extern volatile unsigned char *address;

extern void copro_armnative_tube_interrupt_handler(void);

#endif
