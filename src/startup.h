// startup.h

#ifndef STARTUP_H
#define STARTUP_H

/* Found in the *start.S file, implemented in assembler */

extern unsigned int _software_interrupt_vector_h;
extern unsigned int _fast_interrupt_vector_h;

extern void _init_core();

extern void _spin_core();

extern void _fast_scroll(void *dst, void *src, int num_bytes);

#endif
