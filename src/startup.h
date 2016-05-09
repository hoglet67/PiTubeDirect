// startup.h

#ifndef STARTUP_H
#define STARTUP_H

/* Found in the *start.S file, implemented in assembler */

extern void _start( void );

extern void _enable_interrupts( void );

extern void _disable_interrupts( void );

extern unsigned int _get_cpsr();

extern unsigned int _get_stack_pointer();

extern void _enable_unaligned_access();

extern void _enable_l1_cache();

extern void _invalidate_icache();

extern void _invalidate_dcache();

#endif
